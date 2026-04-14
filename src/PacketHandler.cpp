#include "PacketHandler.h"

void PacketHandler::serialize(const TelemetryData &data, uint8_t *buffer) {
  // Kopiujemy zawartość struktury bezpośrednio do przygotowanego bufora bajtów.
  // Tutaj w przyszłości możesz dodać np. szyfrowanie AES przed skopiowaniem!
  memcpy(buffer, &data, sizeof(TelemetryData));
}

TelemetryData PacketHandler::deserialize(const uint8_t *buffer) {
  TelemetryData data;
  // Odtwarzamy strukturę z odebranych bajtów.
  // Tutaj w przyszłości możesz dodać sprawdzanie, czy dane nie są uszkodzone.
  memcpy(&data, buffer, sizeof(TelemetryData));
  return data;
}

size_t PacketHandler::getPacketSize() { return sizeof(TelemetryData); }