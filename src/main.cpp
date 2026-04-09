#include "GPSManager.h"
#include "LoRaManager.h"
#include "PowerManager.h"
#include <Arduino.h>

// --- DANE LORAWAN ---
uint64_t joinEui = 0x0000000000000000;
uint64_t devEui = 0xe082243450f29654;
uint8_t appKey[] = {0x01, 0x0d, 0xd9, 0xd8, 0xed, 0x9c, 0x97, 0x1f,
                    0xf6, 0xf1, 0x67, 0xd4, 0xe2, 0xdb, 0x8e, 0xe6};

PowerManager power;
GPSManager gpsSystem;
LoRaManager lora(joinEui, devEui, appKey);

void taskGPS(void *pvParameters) {
  for (;;) {
    gpsSystem.update();
    vTaskDelay(pdMS_TO_TICKS(50));
  }
}

void taskLoRa(void *pvParameters) {
  for (;;) {
    lora.process(gpsSystem);
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

void setup() {
  Serial.begin(115200);
  power.begin();
  gpsSystem.begin();
  lora.begin();

  xTaskCreate(taskGPS, "GPS_Task", 4096, NULL, 1, NULL);
  xTaskCreate(taskLoRa, "LoRa_Task", 8192, NULL, 2, NULL);
}

void loop() { vTaskDelete(NULL); }