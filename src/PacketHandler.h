#ifndef PACKET_HANDLER_H
#define PACKET_HANDLER_H

#include <Arduino.h>

#pragma pack(push, 1) // upakuj strukturę bez dodatkowych bajtów wyrównania,
                      // ważne dla transmisji danych
struct TelemetryData {
  uint32_t messageId;
  float latitude;
  float longitude;
  float temperature;
  uint8_t batteryLevel;
};
#pragma pack(pop)

class PacketHandler {
public:
  // Zamienia strukturę na tablicę bajtów gotową do wysłania
  static void serialize(const TelemetryData &data, uint8_t *buffer);

  // Zamienia odebraną tablicę bajtów z powrotem na strukturę
  static TelemetryData deserialize(const uint8_t *buffer);

  // Zwraca stały rozmiar pakietu (przydatne do tworzenia buforów)
  static size_t getPacketSize();
};

#endif // PACKET_HANDLER_H