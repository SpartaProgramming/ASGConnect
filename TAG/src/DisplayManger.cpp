#include "DisplayManager.h"

DisplayManager::DisplayManager()
    : tft(TFT_CS, TFT_DC, TFT_MOSI, TFT_SCLK, TFT_RST) {}

void DisplayManager::begin() {
  pinMode(TFT_BLK, OUTPUT);
  digitalWrite(TFT_BLK, HIGH);

  tft.init(170, 320); // Rozdzielczość ekranu
  tft.setRotation(3); // Orientacja pozioma
  tft.fillScreen(ST77XX_BLACK);
}

void DisplayManager::showBootScreen() {
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(3);
  tft.setCursor(50, 70);
  tft.print("ASG SYSTEM");

  tft.setTextSize(1);
  tft.setCursor(100, 110);
  tft.print("Inicjalizacja...");
}

void DisplayManager::updateTime(uint16_t totalSeconds) {
  uint16_t mins = totalSeconds / 60;
  uint16_t secs = totalSeconds % 60;

  tft.setCursor(210, 35);
  tft.setTextSize(3);

  tft.setTextColor(ST77XX_YELLOW, ST77XX_BLACK);

  tft.printf("%02d:%02d", mins, secs);
}

void DisplayManager::updateGPS(double lat, double lon, GamePhase phase) {
  if (phase != PHASE_RUNNING && phase != PHASE_READY)
    return;

  tft.setTextSize(1);
  tft.setCursor(10, 155);

  if (lat != 0.0 && lon != 0.0) {
    // Nadpisujemy stary tekst z czarnym tłem
    tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
    tft.printf("LAT: %.5f  LON: %.5f", lat, lon);
  } else {
    // Zabezpieczenie wizualne – jeśli zgubi fixa
    tft.setTextColor(ST77XX_RED, ST77XX_BLACK);
    tft.print("SZUKAM SYGNALU GPS...       ");
  }
}

void DisplayManager::updateGameUI(const GameState &state) {
  tft.fillScreen(ST77XX_BLACK);

  tft.setTextSize(2);
  tft.setCursor(5, 5);

  if (state.phase != PHASE_SETUP) {
    if (state.team == TEAM_BLUE)
      tft.setTextColor(ST77XX_BLUE);
    else
      tft.setTextColor(ST77XX_RED);
    tft.print(state.nickname);
  } else {
    tft.setTextColor(ST77XX_WHITE);
    tft.print("BRAK DANYCH");
  }

  // Status połączenia LoRa po prawej
  tft.setCursor(250, 5);
  tft.setTextColor(state.loraConnected ? ST77XX_GREEN : ST77XX_ORANGE);
  tft.print(state.loraConnected ? "L:OK" : "L:--");

  tft.drawLine(0, 25, 320, 25, ST77XX_WHITE);

  // --- 2. GŁÓWNY EKRAN ZALEŻNY OD FAZY GRY ---

  if (state.phase == PHASE_SETUP) {
    tft.setCursor(30, 70);
    tft.setTextSize(3);
    tft.setTextColor(ST77XX_YELLOW);
    tft.print("KONFIGUROWANIE");
  } else if (state.phase == PHASE_READY) {
    tft.setCursor(20, 70);
    tft.setTextSize(3);
    tft.setTextColor(ST77XX_CYAN);
    tft.print("CZEKAJ NA START");
  } else if (state.phase == PHASE_RUNNING) {

    updateTime(state.timeLeftSeconds);

    // B. STATUS GRACZA (Środek)
    tft.setCursor(45, 60);
    tft.setTextSize(4);
    if (state.playerStatus == PLAYER_ALIVE) {
      tft.setTextColor(ST77XX_GREEN);
      tft.print("ALIVE");
    } else {
      tft.setTextColor(ST77XX_RED);
      tft.print("DEAD");
    }

    // C. STATYSTYKI ZESPOŁÓW
    tft.drawLine(0, 110, 320, 110, ST77XX_WHITE);
    tft.setTextSize(2);

    tft.setCursor(10, 120);
    tft.setTextColor(ST77XX_BLUE);
    tft.printf("SOJ: %d/%d", state.alliesAlive, state.alliesTotal);

    tft.setCursor(170, 120);
    tft.setTextColor(ST77XX_RED);
    tft.printf("WRG: %d/%d", state.enemiesAlive, state.enemiesTotal);
  } else if (state.phase == PHASE_ENDED) {
    tft.setCursor(40, 60);
    tft.setTextSize(4);

    // Proste ustawienie koloru na podstawie komunikatu końcowego
    if (state.endMessage.indexOf("BLUE") >= 0)
      tft.setTextColor(ST77XX_BLUE);
    else if (state.endMessage.indexOf("RED") >= 0)
      tft.setTextColor(ST77XX_RED);
    else
      tft.setTextColor(ST77XX_WHITE);

    tft.print(state.endMessage);
  }

  updateGPS(state.lat, state.lon,
            state.phase); // Aktualizujemy GPS przy każdej zmianie UI, aby od
                          // razu pokazać lokalizację
}
