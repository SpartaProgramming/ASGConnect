#include "GPSManager.h"

#define GPS_RX_PIN 34
#define GPS_TX_PIN 12

GPSManager::GPSManager() : serial(1) {}

void GPSManager::begin() {
  // T-Beam GPS zazwyczaj pracuje na 9600 bodów
  serial.begin(9600, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
}

void GPSManager::update() {
  // Czytamy wszystkie dostępne bajty z bufora Serial
  while (serial.available() > 0) {
    gps.encode(serial.read());
  }
}

bool GPSManager::hasFix() {
  // Sprawdzamy czy lokalizacja jest poprawna i czy "wiek" danych jest świeży
  // (poniżej 5s)
  return gps.location.isValid() && gps.location.age() < 5000;
}

double GPSManager::getLat() {
  if (hasFix()) {
    return gps.location.lat();
  }
  return 0.0; // Zwraca 0 jeśli błąd lub brak fixa
}

double GPSManager::getLon() {
  if (hasFix()) {
    return gps.location.lng();
  }
  return 0.0; // Zwraca 0 jeśli błąd lub brak fixa
}