#include "DisplayManager.h"

#define TFT_CS 25
#define TFT_DC 2
#define TFT_MOSI 15
#define TFT_SCLK 14
#define TFT_RST 13
#define TFT_BLK 4

DisplayManager::DisplayManager()
    : tft(TFT_CS, TFT_DC, TFT_MOSI, TFT_SCLK, TFT_RST), _tft_blk(TFT_BLK) {}

bool DisplayManager::begin() {

    pinMode(_tft_blk, OUTPUT);
    digitalWrite(_tft_blk, HIGH); // Załączenie zasilania diod LED w ekranie


  tft.init(170, 320);
  tft.setRotation(3); // Orientacja pozioma (0-3)
  tft.fillScreen(ST77XX_BLACK);

  // tft.invertDisplay(false);
  // tft.setRotation(1);

  tft.fillScreen(ST77XX_BLACK);
  Serial.println(F("[Display] Inicjalizacja ekranu... SUKCES"));
  return true;
}

void DisplayManager::showBootScreen() {
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextColor(ST77XX_GREEN);
  tft.setTextSize(2);
  tft.setCursor(10, 10);
  tft.print("System Start...");

  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(1);
  tft.setCursor(10, 40);
  tft.print("Inicjalizacja RTOS & LoRa");
}

void DisplayManager::updateTelemetryUI(const TelemetryData &data) {
  // Zamiast czyścić cały ekran (co powoduje migotanie),
  // rysujemy czarne prostokąty w miejscu starych wartości (lub używamy
  // specyfiki biblioteki)
  tft.fillRect(0, 60, 320, 100, ST77XX_BLACK);

  tft.setTextColor(ST77XX_CYAN);
  tft.setTextSize(3);

  // Wyświetlanie temperatury
  tft.setCursor(10, 70);
  tft.print("Temp: ");
  tft.print(data.temperature, 1);
  tft.print(" C");

  // Wyświetlanie baterii
  tft.setTextColor(ST77XX_YELLOW);
  tft.setTextSize(2);
  tft.setCursor(10, 120);
  tft.print("Bateria: ");
  tft.print(data.batteryLevel);
  tft.print("%");

  // Wyświetlanie ID pakietu
  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(1);
  tft.setCursor(10, 150);
  tft.print("ID Pakietu: ");
  tft.print(data.messageId);
}

void DisplayManager::showError(const char *message) {
  tft.fillScreen(ST77XX_RED);
  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(2);
  tft.setCursor(10, 50);
  tft.print("BLAD KRYTYCZNY:");
  tft.setTextSize(1);
  tft.setCursor(10, 80);
  tft.print(message);
}