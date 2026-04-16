#ifndef LORA_MANAGER_H
#define LORA_MANAGER_H

#include <Arduino.h>
#include <Preferences.h>
#include <RadioLib.h>
#include <SPI.h>

/**
 * Klasa LoRaManager
 * Zarządza komunikacją LoRaWAN w Klasie A przy użyciu biblioteki RadioLib.
 * Zoptymalizowana pod kątem pracy w środowisku FreeRTOS.
 */
class LoRaManager {
private:
  // --- Obiekty RadioLib (Kolejność ma znaczenie dla inicjalizacji) ---
  Module _mod;       // Moduł sprzętowy (piny)
  SX1262 _radio;     // Sterownik układu radiowego
  LoRaWANNode _node; // Stos protokołu LoRaWAN

  // --- Zarządzanie sesją i stanem ---
  Preferences prefs;          // Pamięć nieulotna ESP32 (NVS)
  uint8_t sessionBuffer[256]; // Bufor na klucze sesyjne i liczniki
  bool isJoined = false;      // Flaga statusu połączenia z siecią

  // --- Dane autoryzacyjne ---
  uint64_t _joinEui;
  uint64_t _devEui;
  uint8_t *_appKey;

  /**
   * Zapisuje bieżący stan sesji (liczniki FCntUp/Down, klucze) do Flash.
   * Zapobiega konieczności ponownego Joinowania po resecie.
   */
  void saveSession();

public:
  /**
   * Konstruktor managera
   * @param joinEui Unikalny identyfikator aplikacji (AppEUI)
   * @param devEui Unikalny identyfikator urządzenia
   * @param appKey Klucz szyfrujący aplikację
   */
  LoRaManager(uint64_t joinEui, uint64_t devEui, uint8_t *appKey);

  /**
   * Inicjalizuje szynę SPI, radio SX1262 oraz stos LoRaWAN.
   * Przywraca sesję z Flash, jeśli istnieje.
   */
  void begin();

  /**
   * Wysyła pakiet danych i nasłuchuje odpowiedzi (Downlink) w oknach RX1/RX2.
   * @param payload Wskaźnik na dane do wysłania
   * @param length Długość danych
   * @param confirmed Czy wymaga potwierdzenia od serwera (ACK)
   * @param rxBuffer Bufor, do którego zostaną zapisane dane odebrane
   * (opcjonalnie)
   * @param rxLen Wskaźnik na zmienną, która otrzyma informację o ilości
   * odebranych bajtów (opcjonalnie)
   * @return true jeśli wysyłka zakończyła się sukcesem (niezależnie od tego czy
   * odebrano downlink)
   */
  bool sendPacket(uint8_t *payload, size_t length, bool confirmed,
                  uint8_t *rxBuffer = nullptr, size_t *rxLen = nullptr);

  /**
   * Sprawdza status połączenia i wykonuje procedurę Join (OTAA), jeśli to
   * konieczne.
   */
  void maintainConnection();

  /**
   * Zwraca aktualny status połączenia.
   */
  bool isConnected() const { return isJoined; }

  /**
   * Metoda zachowana dla kompatybilności wstecznej.
   * W Klasie A downlinki obsługiwane są bezpośrednio w sendPacket.
   */
  void receiveDownlink();
};

#endif // LORA_MANAGER_H