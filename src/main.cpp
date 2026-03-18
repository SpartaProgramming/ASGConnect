#include <Arduino.h>
#include <Preferences.h>
#include <RadioLib.h>
#include <SPI.h>
#include <Wire.h>
#include <XPowersLib.h>

Preferences preferences;

uint8_t noncesBuffer[RADIOLIB_LORAWAN_NONCES_BUF_SIZE];

// --- KONFIGURACJA ZASILANIA (AXP2101) ---
XPowersAXP2101 PMU;
#define I2C_SDA 21
#define I2C_SCL 22

// --- PINY LORA DLA T-BEAM V1.2 (SX1262) ---
#define LORA_CS 18
#define LORA_DIO1 33
#define LORA_RST 23
#define LORA_BUSY 32

// Inicjalizacja modułu radiowego w bibliotece RadioLib
SX1262 radio = new Module(LORA_CS, LORA_DIO1, LORA_RST, LORA_BUSY);

// Tworzymy węzeł LoRaWAN (częstotliwość EU868)
LoRaWANNode node(&radio, &EU868);

// --- KLUCZE CHIRPSTACK (SPRAWDŹ CZY SĄ POPRAWNE) ---
uint64_t joinEui = 0x0000000000000000;
uint64_t devEui = 0xe082243450f29654;

uint8_t appKey[] = {0x01, 0x0d, 0xd9, 0xd8, 0xed, 0x9c, 0x97, 0x1f,
                    0xf6, 0xf1, 0x67, 0xd4, 0xe2, 0xdb, 0x8e, 0xe6};

void setup() {
  Serial.begin(115200);
  while (!Serial)
    ;
  Serial.println(F("--- Start T-Beam ASG Tag (Class-C) ---"));

  // 1. URUCHAMIANIE ZASILANIA (AXP2101)
  Wire.begin(I2C_SDA, I2C_SCL);
  if (PMU.init(Wire, I2C_SDA, I2C_SCL, AXP2101_SLAVE_ADDRESS)) {
    Serial.println(F("PMU: Układ gotowy!"));
    PMU.setALDO2Voltage(3300); // Zasilanie SX1262
    PMU.enableALDO2();
    PMU.setALDO3Voltage(3300); // Zasilanie GPS
    PMU.enableALDO3();
  } else {
    Serial.println(F("BŁĄD: Brak AXP2101!"));
    while (1)
      ;
  }
  delay(1000);

  // 2. KONFIGURACJA MAGISTRALI SPI (NAPRAWIONA!)
  // Usunięto LORA_CS z nawiasu, żeby sprzętowe SPI nie gryzło się z RadioLibem!
  SPI.begin(5, 19, 27);

  // 3. URUCHAMIANIE RADIA SX1262
  Serial.print(F("Inicjalizacja radia... "));
  int state = radio.begin();
  if (state == RADIOLIB_ERR_NONE) {
    Serial.println(F("SUKCES!"));
  } else {
    Serial.print(F("BŁĄD, kod: "));
    Serial.println(state);
    while (1)
      ;
  }

  // POPRAWKI SPRZĘTOWE DLA T-BEAM V1.2
  radio.setTCXO(3.3);               // T-Beam używa kwarcu 3.3V (nie 1.8V!)
  radio.setDio2AsRfSwitch(true);    // Automatyczny przełącznik anteny
  radio.setRxBoostedGainMode(true); // Wzmocnienie czułości odbiornika

  // 1. Otwieramy przestrzeń w pamięci Flash pod nazwą "lorawan"
  preferences.begin("lorawan", false);

  // 2. Sprawdzamy, czy mamy już zapisane liczniki z poprzedniego uruchomienia
  size_t len = preferences.getBytesLength("nonces");
  if (len == RADIOLIB_LORAWAN_NONCES_BUF_SIZE) {
    Serial.println("Wczytuję zapisane liczniki Nonce z pamięci Flash...");
    preferences.getBytes("nonces", noncesBuffer,
                         RADIOLIB_LORAWAN_NONCES_BUF_SIZE);
  } else {
    Serial.println("Brak zapisanych liczników, zaczynamy od zera.");
    // Zerujemy bufor na wszelki wypadek
    memset(noncesBuffer, 0, RADIOLIB_LORAWAN_NONCES_BUF_SIZE);
  }

  // 3. Mówimy bibliotece RadioLib: "Hej, używaj tego bufora do liczników!"
  node.setBufferNonces(noncesBuffer);

  // 4. DOŁĄCZANIE DO SIECI LORAWAN (OTAA)
  Serial.println(F("Konfiguracja kluczy..."));
  node.beginOTAA(joinEui, devEui, appKey, appKey);

  Serial.println(F("Wysyłam żądanie Join..."));
  state = node.activateOTAA();

  // 5. BARDZO WAŻNE: Niezależnie od wyniku (sukces czy błąd), RadioLib podbił
  // licznik. Zapisujemy nowy stan bufora z powrotem do trwałej pamięci Flash!
  preferences.putBytes("nonces", noncesBuffer,
                       RADIOLIB_LORAWAN_NONCES_BUF_SIZE);
  preferences.end(); // Zamykamy dostęp do pamięci

  if (state == RADIOLIB_ERR_NONE) {
    Serial.println(F("SUKCES! Połączono z ChirpStackiem!"));
  } else {
    Serial.print(F("Błąd łączenia, kod: "));
    Serial.println(state);
  }
}

void loop() {
  Serial.println(F("Wysyłam pakiet..."));
  String message = "Hello ASG";
  int state = node.sendReceive((uint8_t *)message.c_str(), message.length());

  if (state == RADIOLIB_ERR_NONE) {
    Serial.println(F("Wysłano!"));
  } else {
    Serial.print(F("Błąd wysyłania: "));
    Serial.println(state);
  }

  // Tymczasowe opóźnienie dla testów
  delay(6000);
}