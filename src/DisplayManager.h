#pragma once
#include "GameProtocol.h"
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h> // Zmień na TFT_eSPI, jeśli używasz tej biblioteki
#include <Arduino.h>

// Definicje pinów dla Twojego ekranu (dostosuj do swojego układu)
#define TFT_CS 15
#define TFT_RST 4
#define TFT_DC 2
#define TFT_MOSI 23
#define TFT_SCLK 18

class DisplayManager {
private:
  Adafruit_ST7789 tft;

public:
  DisplayManager();
  void begin();
  void showBootScreen();

  // Jedyna funkcja potrzebna do rysowania - przyjmuje aktualny Bliźniak Cyfrowy
  void updateGameUI(const GameState &state);
};