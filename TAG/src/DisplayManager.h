#pragma once
#include "GameProtocol.h"
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h> // Zmień na TFT_eSPI, jeśli używasz tej biblioteki
#include <Arduino.h>

// Definicje pinów dla Twojego ekranu (dostosuj do swojego układu)
#define TFT_CS 25
#define TFT_DC 2
#define TFT_MOSI 15
#define TFT_SCLK 14
#define TFT_RST 13
#define TFT_BLK 4

class DisplayManager {
private:
  Adafruit_ST7789 tft;

public:
  DisplayManager();
  void begin();
  void showBootScreen();
  void updateGameUI(const GameState &state);
  void updateTime(uint16_t totalSeconds);
  void updateGPS(double lat, double lon, GamePhase phase);
};