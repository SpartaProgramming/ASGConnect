#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include "PacketHandler.h" // Importujemy strukturę z poprzednich kroków
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <Arduino.h>

class DisplayManager {
private:
  Adafruit_ST7789 tft;
  int _tft_blk; // podswietlanie

public:
  // Konstruktor. Jeśli Twój ekran nie ma pinu CS, przekaż -1
  DisplayManager();

  bool begin();

  // Funkcje do rysowania
  void showBootScreen();
  void updateTelemetryUI(const TelemetryData &data);
  void showError(const char *message);
};

#endif // DISPLAY_MANAGER_H