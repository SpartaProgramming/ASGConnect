#include "PowerManager.h"

#define TFT_CS 25
#define TFT_DC 2
#define TFT_MOSI 15
#define TFT_SCLK 14
#define TFT_RST 13
#define TFT_BLK 4

bool PowerManager::begin() {
  Wire.begin(21, 22);
  Serial.print(F("[PMU] Inicjalizacja AXP2101... "));
  if (PMU.init(Wire, 21, 22, AXP2101_SLAVE_ADDRESS)) {
    PMU.setALDO2Voltage(3300);
    PMU.enableALDO2(); // Radio
    PMU.setALDO3Voltage(3300);
    PMU.enableALDO3(); // GPS
    PMU.setALDO4Voltage(3300);
    PMU.enableALDO4();
    Serial.println(F("SUKCES (Zasilanie włączone)"));
    return true;
  }
  Serial.println(F("BŁĄD! Nie znaleziono układu PMU."));
  return false;
}