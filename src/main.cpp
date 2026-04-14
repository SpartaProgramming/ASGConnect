#include "ButtonManager.h"
#include "DisplayManager.h"
#include "GPSManager.h"
#include "LoRaManager.h"
#include "PowerManager.h"
#include <Arduino.h>
#include <PacketHandler.h>

struct LoRaTxPacket {
  uint8_t payload[10];
  size_t length;
  bool confirmed; // Czy wymaga ACK
};

QueueHandle_t txQueue; // Globalna kolejka do wysyłania pakietów LoRa

// --- DANE LORAWAN ---
uint64_t joinEui = 0x0000000000000000;
uint64_t devEui = 0xe082243450f29654;
uint8_t appKey[] = {0x01, 0x0d, 0xd9, 0xd8, 0xed, 0x9c, 0x97, 0x1f,
                    0xf6, 0xf1, 0x67, 0xd4, 0xe2, 0xdb, 0x8e, 0xe6};

PowerManager power;
GPSManager gpsSystem;
LoRaManager lora(joinEui, devEui, appKey);
DisplayManager display;
GameState gameState;
ButtonManager buttons(36, 39, 35); // READY, KILL, SYNC
unsigned long lastSecondTime = 0;

TaskHandle_t displayTaskHandle = NULL;

void taskGPS(void *pvParameters) {
  for (;;) {
    gpsSystem.update();

    if (gpsSystem.gps.location.isValid() &&
        gpsSystem.gps.location.isUpdated()) {
      gameState.lat = gpsSystem.gps.location.lat();
      gameState.lon = gpsSystem.gps.location.lng();
    }
    vTaskDelay(pdMS_TO_TICKS(50));
  }
}

void taskGame(void *pvParameters) {
  TickType_t xLastWakeTime = xTaskGetTickCount();
  unsigned long lastSecondTime = millis();
  unsigned long lastTelemetryTime = millis();

  for (;;) {
    bool needsDisplayUpdate = false;

    // --- 1. OBSŁUGA PRZYCISKÓW ---
    if (buttons.isKillPressed() && gameState.phase == PHASE_RUNNING &&
        gameState.playerStatus == PLAYER_ALIVE) {
      Serial.println("[BTN] KILL! Zglaszam smierc...");
      gameState.playerStatus = PLAYER_DEAD;
      needsDisplayUpdate = true;

      // Tworzymy pakiet KILLED i wrzucamy do kolejki
      LoRaTxPacket txPacket;
      PacketHandler::buildKilled(txPacket.payload, txPacket.length);
      txPacket.confirmed = true; // Zawsze żądamy potwierdzenia dla KILLED!
      xQueueSend(txQueue, &txPacket, portMAX_DELAY);
    }

    // --- 2. ODLICZANIE CZASU ---
    if (gameState.phase == PHASE_RUNNING) {
      if (millis() - lastSecondTime >= 1000) {
        lastSecondTime = millis();
        if (gameState.timeLeftSeconds > 0) {
          gameState.timeLeftSeconds--;
          needsDisplayUpdate = true;
        }
      }
    } else {
      lastSecondTime = millis();
    }

    // --- 3. WYSYŁANIE TELEMETRII (GPS co 10 sekund) ---
    if (gameState.phase == PHASE_RUNNING) {
      if (millis() - lastTelemetryTime >= 10000) {
        lastTelemetryTime = millis();

        LoRaTxPacket txPacket;
        PacketHandler::buildTelemetry(gameState, txPacket.payload,
                                      txPacket.length);
        txPacket.confirmed = false; // GPS leci w eter bez potwierdzenia

        // Wrzucamy do kolejki (jeśli pełna, nie blokujemy gry, czekamy max 10
        // ticków)
        if (xQueueSend(txQueue, &txPacket, pdMS_TO_TICKS(10)) == pdPASS) {
          Serial.println("[GAME] Telemetria przekazana do radia.");
        }
      }
    }

    // Odśwież ekran jeśli trzeba
    if (needsDisplayUpdate && displayTaskHandle != NULL) {
      xTaskNotifyGive(displayTaskHandle);
    }

    vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(50));
  }
}

void taskLoRa(void *pvParameters) {
  LoRaTxPacket rxFromQueue;
  uint8_t rxBuffer[256];

  for (;;) {
    // 1. Utrzymanie sieci (Join OTAA)
    lora.maintainConnection();

    if (lora.isConnected()) {

      // 2. WYSYŁANIE UPLINKÓW Z KOLEJKI (Timeout = 100ms)
      if (xQueueReceive(txQueue, &rxFromQueue, pdMS_TO_TICKS(100)) == pdPASS) {
        // Jeśli jest pakiet do wysłania, wysyłamy
        lora.sendPacket(rxFromQueue.payload, rxFromQueue.length,
                        rxFromQueue.confirmed);
      }

      size_t rxLen = lora.receivePacket(rxBuffer);
      if (rxLen > 0) {

        PacketHandler::processDownlink(rxBuffer, rxLen, gameState);

        if (displayTaskHandle != NULL) {
          xTaskNotifyGive(
              displayTaskHandle); // Powiadamiamy Display o zmianie stanu gry
        }
      }

    } else {

      vTaskDelay(pdMS_TO_TICKS(5000));
      continue;
    }

    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

void taskDisplay(void *pvParameters) {

  display.updateGameUI(gameState);

  for (;;) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    display.updateGameUI(gameState);
  }
}

void setup() {
  Serial.begin(115200);
  power.begin();
  gpsSystem.begin();
  lora.begin();
  display.begin();
  display.showBootScreen();
  buttons.begin();

  txQueue = xQueueCreate(10, sizeof(LoRaTxPacket));
  if (txQueue == NULL) {
    Serial.println("Blad: Nie udalo sie utworzyc kolejki txQueue!");
  }

  xTaskCreate(taskGPS, "GPS_Task", 4096, NULL, 1, NULL);
  xTaskCreate(taskLoRa, "LoRa_Task", 8192, NULL, 2, NULL);
  xTaskCreate(taskGame, "Game_Task", 4096, NULL, 3, NULL);
  xTaskCreate(taskDisplay, "Display_Task", 4096, NULL, 1, &displayTaskHandle);
}

void loop() { vTaskDelete(NULL); }