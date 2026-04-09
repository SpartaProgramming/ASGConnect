#include "LoRaManager.h"

// zależnosć obiektów: Module(PINY) -> SX1262(Sterownik Chipu) ->
// LoRaWANNode(Stos Protokołu)
LoRaManager::LoRaManager(uint64_t joinEui, uint64_t devEui, uint8_t *appKey)
    : _mod(18, 33, 23, 32),   // LORA_CS, LORA_DIO1, LORA_RST, LORA_BUSY
      _radio(&_mod),          // SX1262 przyjmuje wskaźnik na Module
      _node(&_radio, &EU868), // LoRaWANNode przyjmuje wskaźnik na SX1262
      _joinEui(joinEui), _devEui(devEui), _appKey(appKey) {}

void LoRaManager::begin() {
  SPI.begin(5, 19, 27);

  Serial.print(F("[LoRa] Inicjalizacja radia SX1262... "));
  int state = _radio.begin();
  if (state == RADIOLIB_ERR_NONE) {
    Serial.println(F("OK"));
    _radio.setTCXO(3.3); // oscylator z kompensacją temperaturową
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
  _node.setClass(RADIOLIB_LORAWAN_CLASS_C);
  _node.setBufferSession(sessionBuffer);

  if (_node.isActivated()) {
    Serial.println(F("[LoRa] Urządzenie aktywowane (sesja przywrócona)."));
    isJoined = true;
  } else {
    Serial.println(F("[LoRa] Urządzenie nieaktywne. Wymagany Join."));
  }
}

void LoRaManager::process(GPSManager &gpsManager) {
  static uint32_t lastTxTime = 0;
  const uint32_t TX_INTERVAL = 30000;

  if (!isJoined) {
    Serial.print(F("[LoRa] Wysyłam żądanie Join (OTAA)... "));
    int state =
        _node.activateOTAA(); // start procedury dołączania do sieci -> Request
                              // -> Join Accept -> klucze sesyjne NwkSKey i
                              // AppSKey(do restartu, wygaśnięcia sesji lub
                              // zmiany parametrów sieci)

    if (state == RADIOLIB_ERR_NONE || state == RADIOLIB_LORAWAN_NEW_SESSION) {
      Serial.println(F("POŁĄCZONO!"));
      isJoined = true;
      saveSession();
    } else {
      Serial.printf("NIEUDANE (kod: %d). Następna próba za 10s...\n", state);
      if (state == RADIOLIB_ERR_CHIP_NOT_FOUND)
        Serial.println(F(" -> Sprawdź połączenie SPI z radiem!"));
      vTaskDelay(pdMS_TO_TICKS(10000));
      return;
    }
  }

  // Diagnostyka Downlinku
  uint8_t rxData[256];
  size_t rxLen = 0;
  LoRaWANEvent_t dlEvent;
  int dlState = _node.getDownlinkClassC(rxData, &rxLen, &dlEvent);

  if (dlState == RADIOLIB_ERR_NONE && rxLen > 0) {
    Serial.printf("[LoRa] Otrzymano dane: %d bajtów na porcie %d\n", rxLen,
                  dlEvent.fPort);
  }

  if (millis() - lastTxTime >= TX_INTERVAL) {
    lastTxTime = millis();
    if (gpsManager.gps.location.isValid()) {
      Serial.println(F("[LoRa] Przygotowanie uplink..."));
      uint8_t payload[8];
      // ... (kod pakowania danych bez zmian) ...

      int txState = _node.sendReceive(payload, 8);
      if (txState == RADIOLIB_ERR_NONE) {
        Serial.println(F("[LoRa] Uplink wysłany i potwierdzony."));
        saveSession();
      } else {
        Serial.printf("[LoRa] Błąd wysyłania: %d\n", txState);
        if (txState == RADIOLIB_ERR_NETWORK_NOT_JOINED)
          isJoined = false;
      }
    } else {
      Serial.println(F("[GPS] Brak fixa. Pomijam wysyłanie uplink."));
      // Diagnostyka GPS: wypisz ile satelitów widzi
      Serial.printf("[GPS] Satelity: %d, HDOP: %d\n", // HDOP- Horizontal
                                                      // Dilution of Precision
                    gpsManager.gps.satellites.value(),
                    gpsManager.gps.hdop.value());
    }
  }
}

void LoRaManager::saveSession() {
  Serial.print(F("[LoRa] Zapisuję sesję do Flash... "));
  prefs.begin("lorawan", false);
  size_t written = prefs.putBytes("session", _node.getBufferSession(), 256);
  prefs.end();
  Serial.println(written == 256 ? F("OK") : F("BŁĄD ZAPISU"));
}