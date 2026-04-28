#include "LoRaManager.h"

// Inicjalizacja statycznego mutexu (jeśli dodasz go do klasy w .h)
// SemaphoreHandle_t LoRaManager::spiMutex = xSemaphoreCreateMutex();

LoRaManager::LoRaManager(uint64_t joinEui, uint64_t devEui, uint8_t *appKey)
    : _mod(18, 33, 23, 32), // CS, DIO1, RST, BUSY
      _radio(&_mod), _node(&_radio, &EU868), _joinEui(joinEui), _devEui(devEui),
      _appKey(appKey) {}

void LoRaManager::begin(GameState *statePtr) {
  // 1. Konfiguracja szyny SPI dla T-Beam
  _state = statePtr;
  SPI.begin(5, 19, 27);

  Serial.print(F("[LoRa] Inicjalizacja SX1262... "));
  int state = _radio.begin();
  if (state == RADIOLIB_ERR_NONE) {
    Serial.println(F("OK"));
    // Konfiguracja specyficzna dla modułów T-Beam (TCXO i RF Switch)
    _radio.setTCXO(1.6);
    _radio.setDio2AsRfSwitch(true);
  } else {
    Serial.printf("BŁĄD (%d)\n", state);
  }

  // 2. Wczytywanie sesji z pamięci nieulotnej (NVS)
  prefs.begin("lorawan", false);
  if (prefs.getBytesLength("session") == 256) {
    Serial.println(F("[LoRa] Przywracanie sesji z Flash..."));
    prefs.getBytes("session", sessionBuffer, 256);
  } else {
    Serial.println(F("[LoRa] Brak sesji. Rozpoczynam od zera."));
    memset(sessionBuffer, 0, 256);
  }
  prefs.end();

  // 3. Konfiguracja stosu LoRaWAN
  _node.beginOTAA(_joinEui, _devEui, _appKey, _appKey);
  _node.setADR(true);
  _node.setClass(RADIOLIB_LORAWAN_CLASS_A); // Wymuszamy Klasę A
  _node.setBufferSession(sessionBuffer);

  if (_node.isActivated()) {
    Serial.println(F("[LoRa] Sesja aktywna (przywrócona)."));
    isJoined = true;
  } else {
    Serial.println(F("[LoRa] Wymagana aktywacja OTAA..."));
    isJoined = false;
  }
}

void LoRaManager::maintainConnection() {
  // Jeśli nie jesteśmy połączeni, próbujemy wykonać Join
  if (!isJoined) {
    Serial.print(F("[LoRa] Wysyłanie Join Request (OTAA)... "));

    // Funkcja blokuje na czas trwania procedury Join (ok. 5-6s)
    int state = _node.activateOTAA();

    if (state == RADIOLIB_ERR_NONE || state == RADIOLIB_LORAWAN_NEW_SESSION) {
      Serial.println(F("POŁĄCZONO!"));
      isJoined = true;
      // Sygnał dla reszty systemu
      _state->loraConnected = true;

      saveSession(); // Zapisujemy nową sesję natychmiast
    } else {
      Serial.printf("BŁĄD (%d)\n", state);
      // Delay przed kolejną próbą obsługiwany jest przez Task RTOS
      Serial.println(F("[LoRa] Próba ponownego połączenia za chwilę..."));
      _state->loraConnected = false;
    }
  }
}

bool LoRaManager::sendPacket(uint8_t *payload, size_t length, bool confirmed,
                             uint8_t *rxBuffer, size_t *rxLen) {
  if (!isJoined)
    return false;

  LoRaWANEvent_t dlEvent; // Struktura do przechwycenia informacji o downlinku
                          // (np. fPort, RSSI, multicast)

  int txState =
      _node.sendReceive(payload, length,
                        1,         // fPort (domyślnie 1)
                        rxBuffer,  // Gdzie zapisać downlink
                        rxLen,     // Ile bajtów odebrano
                        confirmed, // Czy serwer ma potwierdzić (ACK)
                        nullptr,   // Event Uplink (opcjonalny)
                        &dlEvent   // Event Downlink (zawiera m.in. fPort)
      );

  // Wynik >= 0 oznacza sukces (wartości dodatnie to konkretne okna RX)
  if (txState >= RADIOLIB_ERR_NONE) {
    saveSession(); // Zapisujemy liczniki ramek (FCntUp)
    return true;
  }

  // Obsługa błędów połączenia
  if (txState == RADIOLIB_ERR_NETWORK_NOT_JOINED) {
    Serial.println(F("[LoRa] Utracono sesję!"));
    isJoined = false;
  } else {
    Serial.printf("[LoRa] Błąd wysyłki: %d\n", txState);
  }

  return false;
}

void LoRaManager::saveSession() {
  static uint32_t lastSavedFCnt = 0;
  uint32_t currentFCnt = _node.getFCntUp();

  // Zapisuj do Flash tylko co 10 wysłanych pakietów, aby chronić pamięć
  if (currentFCnt == 0 || (currentFCnt - lastSavedFCnt) >= 20) {
    prefs.begin("lorawan", false);
    size_t written = prefs.putBytes("session", _node.getBufferSession(), 256);
    prefs.end();

    if (written == 256) {
      lastSavedFCnt = currentFCnt;
      Serial.printf("[NVS] Sesja zapisana. FCntUp: %d\n", currentFCnt);
    } else {
      Serial.println(F("[NVS] BŁĄD zapisu sesji!"));
    }
  }
}
