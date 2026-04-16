#include "PacketHandler.h"

// 0x01 - Telemetria GPS (9 bajtów), pobierana z GameState wartosć lat i lon
void PacketHandler::buildTelemetry(const GameState &state, uint8_t *buffer,
                                   size_t &len) {
  buffer[0] = UL_TELEMETRY; // OpCode dla telemetrii

  int32_t lat_int = (int32_t)(state.lat * 1000000);
  int32_t lon_int = (int32_t)(state.lon * 1000000);

  buffer[1] = (lat_int >> 24) & 0xFF;
  buffer[2] = (lat_int >> 16) & 0xFF;
  buffer[3] = (lat_int >> 8) & 0xFF;
  buffer[4] = lat_int & 0xFF;

  buffer[5] = (lon_int >> 24) & 0xFF;
  buffer[6] = (lon_int >> 16) & 0xFF;
  buffer[7] = (lon_int >> 8) & 0xFF;
  buffer[8] = lon_int & 0xFF;

  len = 9;
}

// 0x02 - KILLED (1 bajt)
void PacketHandler::buildKilled(uint8_t *buffer, size_t &len) {
  buffer[0] = UL_KILLED;
  len = 1;
}

void PacketHandler::processDownlink(uint8_t *payload, size_t len,
                                    GameState &state) {
  if (len == 0)
    return;
  uint8_t opCode = payload[0];

  switch (opCode) {
  case DL_CONFIG: // Konfiguracja gry (faza READY) - zawiera wszystkie
                  // podstawowe informacje o grze
    if (len >= 7) {
      state.phase = PHASE_READY;
      state.team = (payload[1] == 0) ? TEAM_RED : TEAM_BLUE;
      state.gameType = payload[2];
      state.timeLeftSeconds = ((payload[3] << 8) | payload[4]) * 60;
      state.alliesTotal = payload[5];
      state.alliesAlive = payload[5];
      state.enemiesTotal = payload[6];
      state.enemiesAlive = payload[6];

      state.nickname = "";
      for (size_t i = 7; i < len; i++) {
        state.nickname += (char)payload[i];
      }
    }
    break;

  case DL_COMMAND: // Komendy serwera (START, END, WIN, YOU_DIED)
    if (len >= 2) {
      uint8_t cmd = payload[1];
      if (cmd == CMD_START) {
        state.phase = PHASE_RUNNING;
        state.playerStatus = PLAYER_ALIVE;
      } else if (cmd == CMD_END) {
        state.phase = PHASE_ENDED;
        state.endMessage = "CZAS MINAL";
      } else if (cmd == CMD_WIN_BLUE) {
        state.phase = PHASE_ENDED;
        state.endMessage = "BLUE WINS!";
      } else if (cmd == CMD_WIN_RED) {
        state.phase = PHASE_ENDED;
        state.endMessage = "RED WINS!";
      } else if (cmd == CMD_YOU_DIED) {
        state.playerStatus = PLAYER_DEAD;
      }
    }
    break;

  case DL_UPDATE:
    if (len >= 3) {
      state.alliesAlive = payload[1];
      state.enemiesAlive = payload[2];
    }
    break;
  }
}