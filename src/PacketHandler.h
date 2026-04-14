#pragma once
#include "GameProtocol.h"
#include <Arduino.h>

class PacketHandler {
public:
  static void buildTelemetry(const GameState &state, uint8_t *buffer,
                             size_t &len);
  static void buildKilled(uint8_t *buffer, size_t &len);
  static void processDownlink(uint8_t *payload, size_t len, GameState &state);
};