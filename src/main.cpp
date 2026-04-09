

#include <Arduino.h>
#include <Preferences.h>
#include <RadioLib.h>
#include <SPI.h>
#include <TinyGPS++.h>
#include <Wire.h>
#include <XPowersLib.h>

// --- KLUCZE LORAWAN ---
uint64_t joinEui = 0x0000000000000000;
uint64_t devEui = 0xe082243450f29654;
uint8_t appKey[] = {0x01, 0x0d, 0xd9, 0xd8, 0xed, 0x9c, 0x97, 0x1f,
                    0xf6, 0xf1, 0x67, 0xd4, 0xe2, 0xdb, 0x8e, 0xe6};

// --- SPRZĘT I PINY ---
#define I2C_SDA 21
#define I2C_SCL 22
#define LORA_CS 18
#define LORA_DIO1 33
#define LORA_RST 23
#define LORA_BUSY 32
#define GPS_RX_PIN 34
#define GPS_TX_PIN 12

XPowersAXP2101 PMU;
SX1262 radio = new Module(LORA_CS, LORA_DIO1, LORA_RST, LORA_BUSY);
LoRaWANNode node(&radio, &EU868);
Preferences preferences;

TinyGPSPlus gps;
HardwareSerial GPS_Serial(1);

// Automatyczne dopasowanie bufora do wersji RadioLib (v6 lub v7+)
#if defined(RADIOLIB_LORAWAN_SESSION_BUF_SIZE)
uint8_t sessionBuffer[RADIOLIB_LORAWAN_SESSION_BUF_SIZE];
#else
uint8_t sessionBuffer[RADIOLIB_LORAWAN_NONCES_BUF_SIZE];
#endif

bool isJoined = false;

// --- TIMERY ---
uint32_t lastGpsTime = 0;
uint32_t lastPingTime = 0;
const uint32_t GPS_INTERVAL = 20000; // GPS co 20 sekund
const uint32_t PING_INTERVAL = 7000; // Ping co 7 sekund

void saveSession() {
  if (node.isActivated()) {
    preferences.begin("lorawan", false);
#if defined(RADIOLIB_LORAWAN_SESSION_BUF_SIZE)
    uint8_t *persist = node.getBufferSession();
    preferences.putBytes("session", persist, RADIOLIB_LORAWAN_SESSION_BUF_SIZE);
#else
    uint8_t *persist = node.getBufferNonces();
    preferences.putBytes("session", persist, RADIOLIB_LORAWAN_NONCES_BUF_SIZE);
#endif
    preferences.end();
    Serial.println(F(">>> Stan sesji zaktualizowany i zapisany do Flash."));
  }
}

void setup() {
  Serial.begin(115200);
  while (!Serial)
    ;
  Serial.println(F("\n--- T-BEAM V1.2 ASG TRACKER (Z AUTO-RECOVERY) ---"));

  // Zasilanie PMU
  Wire.begin(I2C_SDA, I2C_SCL);
  if (PMU.init(Wire, I2C_SDA, I2C_SCL, AXP2101_SLAVE_ADDRESS)) {
    PMU.setALDO2Voltage(3300);
    PMU.enableALDO2();
    PMU.setALDO3Voltage(3300);
    PMU.enableALDO3();
    PMU.setALDO4Voltage(3300);
    PMU.enableALDO4();
    Serial.println(F("Zasilanie uruchomione."));
  }

  // GPS i Radio
  GPS_Serial.begin(9600, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
  SPI.begin(5, 19, 27);
  radio.begin();
  radio.setTCXO(3.3);
  radio.setDio2AsRfSwitch(true);

  // Wczytywanie pamięci sesji LoRaWAN
  preferences.begin("lorawan", false);
  if (preferences.getBytesLength("session") == sizeof(sessionBuffer)) {
    preferences.getBytes("session", sessionBuffer, sizeof(sessionBuffer));
  } else {
    memset(sessionBuffer, 0, sizeof(sessionBuffer));
  }
  preferences.end();

  node.beginOTAA(joinEui, devEui, appKey, appKey);

#if defined(RADIOLIB_LORAWAN_SESSION_BUF_SIZE)
  node.setBufferSession(sessionBuffer);
#else
  node.setBufferNonces(sessionBuffer);
#endif

  // Aktywacja sieci
  if (node.isActivated()) {
    Serial.println(F("SESJA PRZYWRÓCONA Z FLASH!"));
    isJoined = true;
  } else {
    Serial.println(F("Sesja pusta lub przedawniona - zostawiam zadanie dla "
                     "pętli (Auto-Join)..."));
  }
}

void loop() {
  // 1. Zawsze zbieramy dane GPS w tle
  while (GPS_Serial.available() > 0) {
    gps.encode(GPS_Serial.read());
  }

  uint32_t now = millis();

  // 2. AUTO-RECOVERY: Jeśli straciliśmy sesję, próbujemy ją odnowić (Re-Join)
  // co 10 sekund
  if (!isJoined) {
    static uint32_t lastJoinRetry = 0;
    if (now - lastJoinRetry >= 10000 || lastJoinRetry == 0) {
      lastJoinRetry = now;
      Serial.println(
          F("\n[!] Brak sesji. Negocjuję nowe połączenie OTAA (Join)..."));

      int state = node.activateOTAA();
      if (state == RADIOLIB_ERR_NONE || state == RADIOLIB_LORAWAN_NEW_SESSION) {
        Serial.println(F("[!] JOIN SUKCES! Odzyskałem połączenie."));
        isJoined = true;
        saveSession();
      } else {
        Serial.printf("[!] Błąd Join: %d. Ponowię za 10 sekund...\n", state);
      }
    }
    return; // Wychodzimy z loop() dopóki nie wróci sieć
  }

  // 3. Logika wysyłania (GPS vs Ping)
  uint8_t downData[256];
  size_t downLen = 0;
  int state = RADIOLIB_ERR_NONE;
  bool packetSent = false;

  // A) Pakiet GPS co 20 sekund
  if (now - lastGpsTime >= GPS_INTERVAL) {
    lastGpsTime = now;
    lastPingTime = now;

    uint8_t payload[8] = {0};
    if (gps.location.isValid()) {
      int32_t lat = gps.location.lat() * 100000;
      int32_t lon = gps.location.lng() * 100000;
      payload[0] = (lat >> 24) & 0xFF;
      payload[1] = (lat >> 16) & 0xFF;
      payload[2] = (lat >> 8) & 0xFF;
      payload[3] = lat & 0xFF;
      payload[4] = (lon >> 24) & 0xFF;
      payload[5] = (lon >> 16) & 0xFF;
      payload[6] = (lon >> 8) & 0xFF;
      payload[7] = lon & 0xFF;
      Serial.println(F("📡 Wysyłam GPS (8 bajtów)..."));
    } else {
      Serial.println(F("📡 Brak FIXa GPS. Wysyłam same zera..."));
    }

    state = node.sendReceive(payload, sizeof(payload), 1, downData, &downLen);
    packetSent = true;

    // B) Szybki PING co 7 sekund
  } else if (now - lastPingTime >= PING_INTERVAL) {
    lastPingTime = now;
    Serial.println(F("⚡ Wysyłam szybki PING (1 bajt) - otwieram okno RX..."));
    uint8_t pingPayload[1] = {0x00};
    state = node.sendReceive(pingPayload, sizeof(pingPayload), 1, downData,
                             &downLen);
    packetSent = true;
  }

  // C) Analiza odpowiedzi i wychwytywanie "Zawałów Sesji"
  if (packetSent) {
    if (state == RADIOLIB_ERR_NONE) {
      if (downLen > 0) {
        Serial.println(F("\n====================================="));
        Serial.printf("📥 ODEBRANO ROZKAZ Z BAZY! (Długość: %d)\n", downLen);
        Serial.print(F("Treść: "));
        for (size_t i = 0; i < downLen; i++) {
          Serial.print((char)downData[i]);
        }
        Serial.println(F("\n=====================================\n"));
      } else {
        Serial.println(F("Wysłano OK. Kolejka RX na serwerze pusta."));
      }
      saveSession(); // Zapisujemy zaktualizowane liczniki ramek

    } else {
      Serial.printf("Błąd radiowy: %d\n", state);
      // Łapiemy błędy krytyczne: brak połączenia, przymusowy Re-Join (1116),
      // port invalid
      if (state == RADIOLIB_ERR_NETWORK_NOT_JOINED || state == -1116 ||
          state == -1101 || state == -1104) {
        Serial.println(
            F(">> KRYTYCZNY BŁĄD SESJI. Oznaczam jako niepołączony. <<"));
        isJoined =
            false; // To uruchomi mechanizm Auto-Recovery na początku pętli!
      }
    }
  }
}