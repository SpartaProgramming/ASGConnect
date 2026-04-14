#include "LoRaManager.h"
#include "PacketHandler.h"

bool firstMessageSent = false;

// zależnosć obiektów: Module(PINY) -> SX1262(Sterownik Chipu) ->
// LoRaWANNode(Stos Protokołu)
LoRaManager::LoRaManager(uint64_t joinEui, uint64_t devEui, uint8_t *appKey)
    : _mod(18, 33, 23, 32), // LORA_CS, LORA_DIO1, LORA_RST, LORA_BUSY

      _radio(&_mod),          // SX1262 przyjmuje wskaźnik na Module
      _node(&_radio, &EU868), // LoRaWANNode przyjmuje wskaźnik na SX1262
      _joinEui(joinEui), _devEui(devEui), _appKey(appKey) {}

void LoRaManager::begin() {
  SPI.begin(5, 19, 27);

  Serial.print(F("[LoRa] Inicjalizacja radia SX1262... "));
  int state = _radio.begin();
  if (state == RADIOLIB_ERR_NONE) {
    Serial.println(F("OK"));
    _radio.setTCXO(1.6); // oscylator z kompensacją temperaturową lub 3.3V
    _radio.setDio2AsRfSwitch(
        true); // DIO2 jako sterowanie przełącznikiem antenowym
  } else
    Serial.printf("BŁĄD (kod: %d)\n", state);

  prefs.begin("lorawan", false);
  if (prefs.getBytesLength("session") ==
      256) { // czy w pamieci NVS jest klucz "session" o rozmiarze 256 bajtów
    Serial.println(F("[LoRa] Znaleziono zapisaną sesję w pamięci Flash."));
    prefs.getBytes("session", sessionBuffer, 256); // kopiuj do bufora w RAM
  } else {
    Serial.println(F("[LoRa] Brak zapisanej sesji. Wymagany Join."));
    memset(sessionBuffer, 0, 256);
  }
  prefs.end();

  _node.beginOTAA(
      _joinEui, _devEui, _appKey,
      _appKey); // identyfikator aplikacji, identyfikator urządzenia, klucz
                // sieciowy(szyfrujacy), klucz aplikacyjny

  _node.setADR(true);

  _node.setClass(RADIOLIB_LORAWAN_CLASS_C);
  _node.setBufferSession(sessionBuffer);

  if (_node.isActivated()) {
    Serial.println(F("[LoRa] Urządzenie aktywowane (sesja przywrócona)."));
    isJoined = true;
  } else {
    Serial.println(F("[LoRa] Urządzenie nieaktywne. Wymagany Join."));
  }

  if (isJoined && !firstMessageSent) {
    uint8_t ping = 0x01; // Nasz OpCode dla Telemetrii (nawet z zerami)
    _node.sendReceive(&ping, 1);
    firstMessageSent = true;
  }
}

size_t LoRaManager::receivePacket(uint8_t *buffer) {
  if (!isJoined)
    return 0;

  size_t rxLen = 0;
  LoRaWANEvent_t dlEvent;

  // Dla klasy C radio nasłuchuje bez przerwy
  int dlState = _node.getDownlinkClassC(buffer, &rxLen, &dlEvent);

  if (dlState == RADIOLIB_ERR_NONE && rxLen > 0) {
    Serial.printf("[LoRa] Otrzymano Downlink: %d bajtow na porcie %d\n", rxLen,
                  dlEvent.fPort);
    saveSession();
    return rxLen;
  }

  if (dlState != RADIOLIB_ERR_NONE && dlState != RADIOLIB_ERR_RX_TIMEOUT) {
    Serial.printf("[LoRa] BŁĄD SPRZĘTOWY/PROTOKOŁU: %d\n", dlState);
  }

  return 0;
}

void LoRaManager::maintainConnection() {
  if (!isJoined) {
    firstMessageSent =
        false; // Reset flagi, żeby po ponownym dołączeniu wysłać telemetrię
    Serial.print(F("[LoRa] Wysylam zadanie Join (OTAA)... "));
    int state = _node.activateOTAA();

    saveSession();

    if (state == RADIOLIB_ERR_NONE || state == RADIOLIB_LORAWAN_NEW_SESSION) {
      Serial.println(F("POLACZONO!"));
      isJoined = true;

      uint8_t ping = 0x01; // Nasz OpCode dla Telemetrii (nawet z zerami)
      _node.sendReceive(&ping, 1);
      firstMessageSent = true;

    } else {
      Serial.printf("NIEUDANE (kod: %d). Nastepna proba... \n", state);
    }
  }
}

bool LoRaManager::sendPacket(uint8_t *payload, size_t length, bool confirmed) {
  if (!isJoined)
    return false;

  // 1. Wychodzimy z ciągłego nasłuchu (Klasa C) do trybu czuwania z
  // podtrzymaniem kwarcu
  _radio.standby(RADIOLIB_SX126X_STANDBY_XOSC);

  // 2. Najprostsza, działająca wersja wysyłki
  // Parametry: bufor danych, długość, port (domyślnie 1), czy wymaga
  // potwierdzenia (ACK)
  int txState = _node.sendReceive(payload, length, 1, confirmed);

  if (txState == RADIOLIB_ERR_NONE || txState == RADIOLIB_LORAWAN_NEW_SESSION) {
    Serial.println(F("[LoRa] Uplink wysłany pomyślnie!"));
    saveSession();
    return true;
  } else {
    Serial.printf("[LoRa] Błąd wysyłania: %d\n", txState);
    return false;
  }
}

void LoRaManager::saveSession() {
  static uint32_t lastSavedFrameCount = 0;
  uint32_t currentFrameCount = _node.getFCntUp();

  if (currentFrameCount == 0 ||
      (currentFrameCount - lastSavedFrameCount) >= 20) {
    Serial.print(F("[LoRa] Zapisuję sesję do Flash (FCntUp: "));
    Serial.print(currentFrameCount);
    Serial.print(F(")... "));

    prefs.begin("lorawan", false);
    size_t written = prefs.putBytes("session", _node.getBufferSession(), 256);
    prefs.end();

    if (written == 256) {
      lastSavedFrameCount = currentFrameCount;
      Serial.println(F("OK"));
    } else {
      Serial.println(F("BŁĄD"));
    }
  }
}

void LoRaManager::receiveDownlink() {
  if (!isJoined) return;

  uint8_t rxBuffer[256];
  size_t rxLen = 0;
  LoRaWANEvent_t dlEvent;

  // Funkcja sprawdza, czy w tle odebrano i zdekodowano pakiet w oknie RXC
  int16_t state = _node.getDownlinkClassC(rxBuffer, &rxLen, &dlEvent);

  // W RadioLib dla getDownlinkClassC zwrócenie wartości > 0 (najczęściej 3, oznaczające okno RXC) to sukces
  if (state > 0) {
    if (rxLen > 0) {
      Serial.printf("[LoRa] Odebrano Downlink Klasy C! Długość: %d bajtów\n", rxLen);
      Serial.print("[LoRa] Payload: ");
      for (size_t i = 0; i < rxLen; i++) {
        Serial.printf("%02X ", rxBuffer[i]);
      }
      Serial.println();
    } else {
      Serial.println("[LoRa] Odebrano pusty downlink (np. komenda sieciowa MAC lub same potwierdzenie).");
    }
  } 
  // RADIOLIB_ERR_NONE (0) oznacza brak pakietu - naturalny stan podczas nasłuchu.
  // RADIOLIB_ERR_DOWNLINK_MALFORMED często pojawia się przy szumie radiowym - ignorujemy go.
  else if (state != RADIOLIB_ERR_NONE && state != RADIOLIB_ERR_DOWNLINK_MALFORMED) {
     Serial.printf("[LoRa] Błąd odbioru Klasy C: %d\n", state);
  }
}