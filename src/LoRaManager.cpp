#include "LoRaManager.h"
#include "PacketHandler.h"

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
  return 0;
}

void LoRaManager::maintainConnection() {
  if (!isJoined) {
    Serial.print(F("[LoRa] Wysylam zadanie Join (OTAA)... "));
    int state = _node.activateOTAA();

    saveSession();

    if (state == RADIOLIB_ERR_NONE || state == RADIOLIB_LORAWAN_NEW_SESSION) {
      Serial.println(F("POLACZONO!"));
      isJoined = true;
    } else {
      Serial.printf("NIEUDANE (kod: %d). Nastepna proba... \n", state);
    }
  }
}

bool LoRaManager::sendPacket(uint8_t *payload, size_t length, bool confirmed) {
  if (!isJoined)
    return false;

  // W RadioLib sendReceive domyślnie przyjmuje: (dane, długość, port,
  // isConfirmed) Port = 1 (domyślny port aplikacji dla naszych OpCodes)
  int txState = _node.sendReceive(payload, length, 1, confirmed);

  if (txState == RADIOLIB_ERR_NONE || txState == RADIOLIB_LORAWAN_NEW_SESSION) {
    Serial.printf("[LoRa] Uplink wyslany! Rozmiar: %d bajtow\n", length);
    saveSession();
    return true;
  } else {
    Serial.printf("[LoRa] Blad wysylania: %d\n", txState);
    if (txState == RADIOLIB_ERR_NETWORK_NOT_JOINED) {
      isJoined = false; // Wymuszamy ponowny Join
    }
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