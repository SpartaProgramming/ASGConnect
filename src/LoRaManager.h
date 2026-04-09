#ifndef LORA_MANAGER_H
#define LORA_MANAGER_H

#include "GPSManager.h"
#include <Preferences.h>
#include <RadioLib.h>

class LoRaManager {
private:
  // WAŻNE: Kolejność deklaracji określa kolejność inicjalizacji
  Module _mod;
  SX1262 _radio;
  LoRaWANNode _node;

  Preferences prefs;
  uint8_t sessionBuffer[256];
  bool isJoined = false;

  uint64_t _joinEui;
  uint64_t _devEui;
  uint8_t *_appKey;

  void saveSession();

public:
  LoRaManager(uint64_t joinEui, uint64_t devEui, uint8_t *appKey);
  void begin();
  void process(GPSManager &gpsManager);
};

#endif