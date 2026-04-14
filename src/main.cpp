// #include "ButtonManager.h"
// #include "DisplayManager.h"
// #include "GPSManager.h"
// #include "LoRaManager.h"
// #include "PowerManager.h"
// #include <Arduino.h>
// #include <PacketHandler.h>

// struct LoRaTxPacket {
//   uint8_t payload[10];
//   size_t length;
//   bool confirmed; // Czy wymaga ACK
// };

// QueueHandle_t txQueue; // Globalna kolejka do wysyłania pakietów LoRa

// // --- DANE LORAWAN ---
// uint64_t joinEui = 0x0000000000000000;
// uint64_t devEui = 0xe082243450f29654;
// uint8_t appKey[] = {0x01, 0x0d, 0xd9, 0xd8, 0xed, 0x9c, 0x97, 0x1f,
//                     0xf6, 0xf1, 0x67, 0xd4, 0xe2, 0xdb, 0x8e, 0xe6};

// PowerManager power;
// GPSManager gpsSystem;
// LoRaManager lora(joinEui, devEui, appKey);
// DisplayManager display;
// GameState gameState;
// ButtonManager buttons(36, 39, 35); // READY, KILL, SYNC
// unsigned long lastSecondTime = 0;

// TaskHandle_t displayTaskHandle = NULL;

// void taskGPS(void *pvParameters) {
//   for (;;) {
//     gpsSystem.update();

//     if (gpsSystem.gps.location.isValid() &&
//         gpsSystem.gps.location.isUpdated()) {
//       gameState.lat = gpsSystem.gps.location.lat();
//       gameState.lon = gpsSystem.gps.location.lng();
//     }
//     vTaskDelay(pdMS_TO_TICKS(50));
//   }
// }

// void taskGame(void *pvParameters) {
//   TickType_t xLastWakeTime = xTaskGetTickCount();
//   unsigned long lastSecondTime = millis();
//   unsigned long lastTelemetryTime = millis();

//   for (;;) {
//     bool needsDisplayUpdate = false;

//     // --- 1. OBSŁUGA PRZYCISKÓW ---
//     if (buttons.isKillPressed() && gameState.phase == PHASE_RUNNING &&
//         gameState.playerStatus == PLAYER_ALIVE) {
//       Serial.println("[BTN] KILL! Zglaszam smierc...");
//       gameState.playerStatus = PLAYER_DEAD;
//       needsDisplayUpdate = true;

//       // Tworzymy pakiet KILLED i wrzucamy do kolejki
//       LoRaTxPacket txPacket;
//       PacketHandler::buildKilled(txPacket.payload, txPacket.length);
//       txPacket.confirmed = true; // Zawsze żądamy potwierdzenia dla KILLED!
//       xQueueSend(txQueue, &txPacket, portMAX_DELAY);
//     }

//     // --- 2. ODLICZANIE CZASU ---
//     if (gameState.phase == PHASE_RUNNING) {
//       if (millis() - lastSecondTime >= 1000) {
//         lastSecondTime = millis();
//         if (gameState.timeLeftSeconds > 0) {
//           gameState.timeLeftSeconds--;
//           needsDisplayUpdate = true;
//         }
//       }
//     } else {
//       lastSecondTime = millis();
//     }

//     // --- 3. WYSYŁANIE TELEMETRII (GPS co 10 sekund) ---
//     if (gameState.phase == PHASE_RUNNING) {
//       if (millis() - lastTelemetryTime >= 10000) {
//         lastTelemetryTime = millis();

//         LoRaTxPacket txPacket;
//         PacketHandler::buildTelemetry(gameState, txPacket.payload,
//                                       txPacket.length);
//         txPacket.confirmed = false; // GPS leci w eter bez potwierdzenia

//         // Wrzucamy do kolejki (jeśli pełna, nie blokujemy gry, czekamy max
//         10
//         // ticków)
//         if (xQueueSend(txQueue, &txPacket, pdMS_TO_TICKS(10)) == pdPASS) {
//           Serial.println("[GAME] Telemetria przekazana do radia.");
//         }
//       }
//     }

//     // Odśwież ekran jeśli trzeba
//     if (needsDisplayUpdate && displayTaskHandle != NULL) {
//       xTaskNotifyGive(displayTaskHandle);
//     }

//     vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(50));
//   }
// }

// void taskLoRa(void *pvParameters) {
//   LoRaTxPacket rxFromQueue;
//   uint8_t rxBuffer[256];

//   for (;;) {
//     if (digitalRead(26) == HIGH || digitalRead(26) == HIGH) {
//       Serial.println("!!! [Fizyka] DIO1 SYGNAŁ WYKRYTY! Radio coś widzi
//       !!!");
//     }

//     if (lora.isConnected()) {
//       // 1. ODBIÓR DOWNLINKU ASYNCHRONICZNEGO (Klasa C)
//       size_t rxLen = lora.receivePacket(rxBuffer);
//       if (rxLen > 0) {
//         Serial.println("[LoRa] DANE ODEBRANE Z CHIRPSTACKA (Klasa C)!");
//         PacketHandler::processDownlink(rxBuffer, rxLen, gameState);
//         if (displayTaskHandle != NULL) {
//           xTaskNotifyGive(displayTaskHandle);
//         }
//       }

//       // 2. WYSYŁANIE UPLINKÓW (z nasłuchem w RX1/RX2)
//       if (xQueueReceive(txQueue, &rxFromQueue, 0) == pdPASS) {
//         size_t dlLen = 0; // Inicjalizujemy długość zera

//         // rxBuffer mamy już utworzony na początku taska (uint8_t
//         // rxBuffer[256];)
//         bool success = lora.sendPacket(rxFromQueue.payload,
//         rxFromQueue.length,
//                                        rxFromQueue.confirmed, rxBuffer,
//                                        &dlLen);

//         // Jeżeli success jest true i dlLen > 0, to znaczy że mamy pakiet!
//         if (success && dlLen > 0) {
//           Serial.println("[LoRa] DANE ODEBRANE Z CHIRPSTACKA (Złapane od razu
//           "
//                          "po Uplinku)!");
//           PacketHandler::processDownlink(rxBuffer, dlLen, gameState);
//           if (displayTaskHandle != NULL) {
//             xTaskNotifyGive(displayTaskHandle);
//           }
//         }
//       }
//     } else {
//       lora.maintainConnection();
//       vTaskDelay(pdMS_TO_TICKS(5000));
//       continue;
//     }

//     // Pozwól Watchdogowi i bibliotece LoRa "odetchnąć"
//     vTaskDelay(pdMS_TO_TICKS(10));
//   }
// }

// void taskDisplay(void *pvParameters) {

//   display.updateGameUI(gameState);

//   for (;;) {
//     ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

//     display.updateGameUI(gameState);
//   }
// }

// void setup() {
//   Serial.begin(115200);
//   power.begin();
//   gpsSystem.begin();
//   lora.begin();
//   display.begin();
//   display.showBootScreen();
//   Serial.println("System uruchomiony. Czekam na połączenie LoRaWAN...");
//   buttons.begin();

//   txQueue = xQueueCreate(10, sizeof(LoRaTxPacket));
//   if (txQueue == NULL) {
//     Serial.println("Blad: Nie udalo sie utworzyc kolejki txQueue!");
//   }

//   // xTaskCreate(taskGPS, "GPS_Task", 4096, NULL, 1, NULL);
//   xTaskCreate(taskLoRa, "LoRa_Task", 8192, NULL, 3, NULL);
//   // xTaskCreate(taskGame, "Game_Task", 4096, NULL, 2, NULL);
//   // xTaskCreate(taskDisplay, "Display_Task", 4096, NULL, 1,
//   // &displayTaskHandle);
// }

// void loop() { vTaskDelete(NULL); }

#include "LoRaManager.h"
#include "PowerManager.h"
#include <Arduino.h>

// Definicje pinów dla SX1262 na T-Beam V1.2
#define LORA_DIO1 33
#define LORA_BUSY 32
#define LORA_RST 23
#define LORA_CS 18

PowerManager power;
// Ważne: Nie inicjalizujemy DisplayManager, aby nie dotykać pinów 18/23!

// Dane LoRaWAN (Twoje klucze)
uint64_t joinEui = 0x0000000000000000;
uint64_t devEui = 0xe082243450f29654;
uint8_t appKey[] = {0x01, 0x0d, 0xd9, 0xd8, 0xed, 0x9c, 0x97, 0x1f,
                    0xf6, 0xf1, 0x67, 0xd4, 0xe2, 0xdb, 0x8e, 0xe6};

LoRaManager lora(joinEui, devEui, appKey);

void debugPins() {
  static unsigned long lastDebug = 0;
  if (millis() - lastDebug > 500) { // Co pół sekundy wypisz stan
    lastDebug = millis();
    Serial.printf(
        "[PINS] DIO1(33): %d | BUSY(32): %d | RST(23): %d | CS(18): %d\n",
        digitalRead(LORA_DIO1), digitalRead(LORA_BUSY), digitalRead(LORA_RST),
        digitalRead(LORA_CS));
  }
}

void taskLoRa(void *pvParameters) {
  uint8_t rxBuffer[256];
  uint8_t testPayload[] = {0xDE, 0xAD, 0xBE, 0xEF};
  unsigned long lastTxTime = 0;
  int txCounter = 0;

  for (;;) {

    // 1. AUTOMATYCZNE WYSYŁANIE (Zmienione na 30s - Klasa C nie lubi spamu)
    if (lora.isConnected() && (millis() - lastTxTime > 30000)) {
      lastTxTime = millis();
      txCounter++;

      Serial.printf("\n[TX] Rozpoczynam wysyłanie pakietu testowego #%d...\n",
                    txCounter);

      bool success = lora.sendPacket(testPayload, sizeof(testPayload), false);

      if (success) {
        Serial.println(F("[TX] Sukces! Pakiet wysłany."));
      } else {
        Serial.printf("[TX] Błąd wysyłania! (Sprawdź logi)\n");
      }
    }

    // 2. CIĄGŁY NASŁUCH (Klasa C)
    if (lora.isConnected()) {
      size_t rxLen = lora.receivePacket(rxBuffer);
      if (rxLen > 0) {
        Serial.printf("\n[RX] ODEBRANO DOWNLINK: %d bajtów!\n", rxLen);
        for (size_t i = 0; i < rxLen; i++)
          Serial.printf("%02X ", rxBuffer[i]);
        Serial.println();
      }
    } else {
      lora.maintainConnection();
      vTaskDelay(pdMS_TO_TICKS(5000));
      continue;
    }

    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000); // Czas na otwarcie monitora
  Serial.println(F("--- START DEBUG MODE (SX1262) ---"));

  // 1. Zasilanie (kluczowe dla SX1262)
  if (!power.begin()) {
    Serial.println(F("BŁĄD PMU! Radio może nie mieć prądu."));
  }

  // 2. Inicjalizacja LoRa
  // Upewnij się, że w lora.begin() masz setTCXO(1.6) i setDio2AsRfSwitch(true)
  lora.begin();

  // Tworzymy tylko jeden task, żeby nic nie zakłócało SPI
  xTaskCreate(taskLoRa, "LoRa_Task", 8192, NULL, 5, NULL);
}

void loop() { vTaskDelete(NULL); }