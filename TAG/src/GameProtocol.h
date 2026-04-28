#pragma once
#include <Arduino.h>

/**
 *
 * PROTOKÓŁ KOMUNIKACJI (Serwer <-> Tag)
 * Definiuje format danych wysyłanych między serwerem a tagiem, zarówno w górę
 * (uplink), jak i w dół (downlink).
 */

// --- 1. GLOBALNA FAZA GRY ---
enum GamePhase {
  PHASE_SETUP = 0,
  PHASE_READY = 1,
  PHASE_RUNNING = 2,
  PHASE_ENDED = 3
};

// --- 2. LOKALNY STATUS GRACZA ---
enum PlayerStatus { PLAYER_ALIVE = 0, PLAYER_DEAD = 1 };

enum TeamColor { TEAM_RED = 0, TEAM_BLUE = 1 };

// --- OPCODES ---
#define DL_CONFIG 0x01
#define DL_COMMAND 0x02
#define DL_UPDATE 0x03

#define CMD_START 0x10
#define CMD_END 0x11
#define CMD_WIN_BLUE 0x12
#define CMD_WIN_RED 0x13
#define CMD_YOU_DIED 0x14

#define UL_TELEMETRY 0x01
#define UL_KILLED 0x02

// --- CYFROWY BLIŹNIAK ---
struct GameState {
  GamePhase phase = PHASE_SETUP;
  PlayerStatus playerStatus =
      PLAYER_DEAD; // Startujemy jako martwi, serwer nas ożywi
  TeamColor team = TEAM_RED;
  String nickname = "PLAYER";
  uint8_t gameType = 0;

  uint16_t timeLeftSeconds = 0;

  uint8_t alliesAlive = 0;
  uint8_t alliesTotal = 0;
  uint8_t enemiesAlive = 0;
  uint8_t enemiesTotal = 0;

  double lat = 0.0;
  double lon = 0.0;
  uint8_t battery = 100;

  bool loraConnected = false;
  String endMessage = "";
};