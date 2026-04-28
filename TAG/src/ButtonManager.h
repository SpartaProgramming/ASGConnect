#ifndef BUTTON_MANAGER_H
#define BUTTON_MANAGER_H

#include <Arduino.h>

class ButtonManager {
private:
  struct BtnState {
    uint8_t pin;
    bool lastReading; // ostatni odczytany stan (do wykrywania zmian)
    bool state;       // aktualny stabilny stan
    unsigned long lastDebounceTime; // czas ostatniej zmiany stanu
  };

  BtnState btnReady;
  BtnState btnKill;
  BtnState btnSync;

  unsigned long debounceDelay;

  // Wewnętrzna funkcja do debouncingu
  bool checkPress(BtnState &btn);

public:
  // Konstruktor z możliwością zmiany pinów
  ButtonManager(uint8_t pinReady = 33, uint8_t pinKill = 32,
                uint8_t pinSync = 35);

  void begin();

  // Metody zwracające true tylko w momencie kliknięcia (raz na naciśnięcie)
  bool isReadyPressed();
  bool isKillPressed();
  bool isSyncPressed();
};

#endif