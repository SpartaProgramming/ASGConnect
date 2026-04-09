#include "GPSManager.h"

#define GPS_RX_PIN 34
#define GPS_TX_PIN 12

GPSManager::GPSManager() : serial(1) {}

void GPSManager::begin() {
  serial.begin(9600, SERIAL_8N1, GPS_RX_PIN,
               GPS_TX_PIN); // otwarcie portu szeregowego dla GPS, 8N1 - 8 bitów
                            // danych, brak parzystości, 1 bit stopu
}

void GPSManager::update() { // moduł wysyła dane w formacie NMEA, więc
                            // odczytujemy je znak po znaku i przekazujemy do
                            // TinyGPS++ do parsowania
  while (serial.available() > 0) {
    gps.encode(serial.read()); // czyta znak po znaku, składa zdanie i wysyła do
                               // parsowania, aktualizacja struktur
  }
}