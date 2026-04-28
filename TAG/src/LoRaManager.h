#ifndef LORA_MANAGER_H
#define LORA_MANAGER_H

#include "GameProtocol.h"
#include <Arduino.h>
#include <Preferences.h>
#include <RadioLib.h>
#include <SPI.h>

class LoRaManager {
private:
  // --- Obiekty RadioLib (Kolejność ma znaczenie dla inicjalizacji) ---
  Module _mod;       // Moduł sprzętowy (piny)
  SX1262 _radio;     // Sterownik układu radiowego
  LoRaWANNode _node; // Stos protokołu LoRaWAN

  GameState *_state;

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
  void begin(GameState *statePtr);

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
};

#endif // LORA_MANAGER_H