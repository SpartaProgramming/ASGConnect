#include "ButtonManager.h"

ButtonManager::ButtonManager(uint8_t pinReady, uint8_t pinKill,
                             uint8_t pinSync) {
  btnReady = {pinReady, HIGH, HIGH, 0};
  btnKill = {pinKill, HIGH, HIGH, 0};
  btnSync = {pinSync, HIGH, HIGH, 0};

  debounceDelay = 80; // 50ms wystarczy do standardowego debouncingu
}

void ButtonManager::begin() {

  pinMode(btnReady.pin, INPUT_PULLUP);
  pinMode(btnKill.pin, INPUT_PULLUP);
  pinMode(btnSync.pin, INPUT_PULLUP);
}

bool ButtonManager::checkPress(BtnState &btn) {
  bool reading = digitalRead(btn.pin);
  bool triggered = false;

  // Zresetuj timer, jeśli stan się zmienił (nawet od drgań styków)
  if (reading != btn.lastReading) {
    btn.lastDebounceTime = millis();
  }

  // Jeśli minął czas debouncingu i stan jest stabilny
  if ((millis() - btn.lastDebounceTime) > debounceDelay) {
    if (reading !=
        btn.state) { // Stan się ustabilizował i jest inny niż poprzednio
      btn.state = reading;

      // Wykrywamy tylko naciśnięcie (stan niski - przycisk zwiera do GND)
      if (btn.state == LOW) {
        triggered = true;
      }
    }
  }

  btn.lastReading = reading;
  return triggered; // zwraca true tylko w momencie wykrycia naciśnięcia
                    // HIGH->LOW
}

bool ButtonManager::isReadyPressed() { return checkPress(btnReady); }

bool ButtonManager::isKillPressed() { return checkPress(btnKill); }

bool ButtonManager::isSyncPressed() { return checkPress(btnSync); }