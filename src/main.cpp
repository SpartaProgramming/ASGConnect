#include "ButtonManager.h"
#include "DisplayManager.h"
#include "GPSManager.h"
#include "LoRaManager.h"
#include "PacketHandler.h"
#include "PowerManager.h"
#include <Arduino.h>

uint64_t joinEui = 0x0000000000000000;
uint64_t devEui = 0xe082243450f29654;
uint8_t appKey[] = {0x01, 0x0d, 0xd9, 0xd8, 0xed, 0x9c, 0x97, 0x1f,
                    0xf6, 0xf1, 0x67, 0xd4, 0xe2, 0xdb, 0x8e, 0xe6};

/**
 * --- STRUKTURY DANYCH ---
 */
struct LoRaTxPacket {
  uint8_t payload[64];
  size_t length;
  bool confirmed;
};

// Globalne obiekty i handlery
LoRaManager lora(joinEui, devEui, appKey);
PowerManager power;
GameState gameState;
QueueHandle_t txQueue;
TaskHandle_t displayTaskHandle = NULL;
DisplayManager display;
ButtonManager buttons(36, 39, 35); // Ready=38, Kill=37, Sync=39
GPSManager gps;

void taskDisplay(void *pvParameters) {
  display.begin();
  display.showBootScreen();
  display.updateGameUI(gameState);

  for (;;) {
    if (ulTaskNotifyTake(pdTRUE, portMAX_DELAY)) {
      display.updateGameUI(gameState);
    }
  }
}

void taskGPS(void *pvParameters) {
  gps.begin();
  for (;;) {
    gps.update();

    static uint32_t lastSend = 0;
    if (millis() - lastSend > 10000) {
      lastSend = millis();

      gameState.lat = gps.getLat();
      gameState.lon = gps.getLon();
      Serial.printf("[GPS Task] Lat: %f, Lon: %f\n", gameState.lat,
                    gameState.lon);

      if (gameState.lat != 0.0 || true) {
        LoRaTxPacket msg;
        PacketHandler::buildTelemetry(gameState, msg.payload, msg.length);
        msg.confirmed = false;
        xQueueSend(txQueue, &msg, 0);
      }
    }
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

void taskButtons(void *pvParameters) {
  buttons.begin();

  for (;;) {
    // 1. Przycisk READY (Potwierdzenie gotowości do gry)
    if (buttons.isReadyPressed()) {
      // Logika: Reaguj tylko, gdy faza gry na to pozwala
      if (gameState.phase == PHASE_SETUP || gameState.phase == PHASE_READY) {
        Serial.println("[BTN] READY Pressed - Sending to LoRa");
        LoRaTxPacket msg;
        msg.payload[0] = 0x00;
        msg.length = 1;
        msg.confirmed = true;
        xQueueSend(txQueue, &msg, 0);

        gameState.endMessage = "WAIT FOR START";
        xTaskNotifyGive(displayTaskHandle);
      } else {
        // Teraz ten napis pojawi się TYLKO, gdy faktycznie naciśniesz przycisk
        // w złym momencie
        Serial.println("[BTN] Ready pressed but IGNORED (wrong game phase)");
      }
    }

    // 2. Przycisk KILL (Zgłoszenie eliminacji)
    if (buttons.isKillPressed()) {
      Serial.println("[BTN] KILL Pressed!");
      LoRaTxPacket msg;
      PacketHandler::buildKilled(msg.payload, msg.length);
      msg.confirmed = true; // Śmierć musi dotrzeć do serwera
      xQueueSend(txQueue, &msg, 0);
    }

    // 3. Przycisk SYNC (Wymuszenie wysyłki/odbioru - PING)
    if (buttons.isSyncPressed()) {
      Serial.println("[BTN] SYNC Pressed!");
      LoRaTxPacket msg;
      msg.payload[0] = 0x03; // Opcode PING
      msg.length = 1;
      msg.confirmed = false;
      xQueueSend(txQueue, &msg, 0);
    }

    vTaskDelay(pdMS_TO_TICKS(20)); // Szybkie sprawdzanie co 20ms
  }
}

void taskLoRa(void *pvParameters) {
  LoRaTxPacket txData;
  uint8_t rxBuffer[256];
  size_t rxLen = 0;

  for (;;) {
    lora.maintainConnection();

    if (lora.isConnected()) {

      if (xQueueReceive(txQueue, &txData, pdMS_TO_TICKS(1000)) == pdPASS) {
        Serial.printf("[LoRa Task] Wysyłam pakiet (%d bajtów)...\n",
                      txData.length);

        rxLen = 0; // Reset długości przed odbiorem
        bool success = lora.sendPacket(txData.payload, txData.length,
                                       txData.confirmed, rxBuffer, &rxLen);

        if (success) {
          Serial.println(F("[LoRa] OK - wysłano"));
        } else {
          Serial.print(F("[LoRa] BŁĄD WYSYŁKI!"));
        }

        if (success) {
          Serial.println(F("[LoRa Task] Uplink wysłany pomyślnie."));

          if (rxLen > 0) { // Otrzymano downlink
            Serial.printf("[LoRa Task] ODEBRANO DOWNLINK: %d bajtów\n", rxLen);
            PacketHandler::processDownlink(rxBuffer, rxLen, gameState);

            if (displayTaskHandle != NULL) {
              xTaskNotifyGive(displayTaskHandle);
            }
          }
        } else {
          Serial.println(F("[LoRa Task] Błąd wysyłania."));
        }
      }
    } else {
      vTaskDelay(pdMS_TO_TICKS(5000));
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println(F("--- SYSTEM START (RTOS CLASS A) ---"));

  // Inicjalizacja sprzętu
  power.begin();
  lora.begin();

  // Tworzenie kolejki (max 10 oczekujących pakietów)
  txQueue = xQueueCreate(10, sizeof(LoRaTxPacket));

  if (txQueue == NULL) {
    Serial.println(F("KRYTYCZNY BŁĄD: Nie utworzono kolejki!"));
    while (1)
      ;
  }

  xTaskCreatePinnedToCore(taskLoRa, "LoRaTask", 8192, NULL, 3, NULL, 1);
  xTaskCreatePinnedToCore(taskGPS, "GPSTask", 4096, NULL, 2, NULL, 1);
  xTaskCreatePinnedToCore(taskButtons, "BtnTask", 4096, NULL, 2, NULL, 1);
  xTaskCreatePinnedToCore(taskDisplay, "DispTask", 4096, NULL, 1,
                          &displayTaskHandle, 1);
}

void loop() {
  // W RTOS pętla loop musi być pusta lub usunięta
  vTaskDelete(NULL);
}