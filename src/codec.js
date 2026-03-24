// Rozkodowywanie wiadomości OD urządzenia (T-Beam -> ChirpStack)
function decodeUplink(input) {
  let bytes = input.bytes;
  let decoded = {};

  // 1. Sprawdzamy czy to krótki PING (1 bajt)
  if (bytes.length === 1) {
    decoded.type = 'ping';
    decoded.message = 'Otwarcie okna RX';
    return {data: decoded};
  }

  // 2. Sprawdzamy czy to pełny pakiet GPS (8 bajtów)
  if (bytes.length === 8) {
    // Operacje bitowe w JS poprawnie zachowują znak minus (Signed 32-bit
    // Integer)
    let latRaw =
        (bytes[0] << 24) | (bytes[1] << 16) | (bytes[2] << 8) | bytes[3];
    let lonRaw =
        (bytes[4] << 24) | (bytes[5] << 16) | (bytes[6] << 8) | bytes[7];

    decoded.type = 'gps';

    // Jeśli same zera (brak fixa)
    if (latRaw === 0 && lonRaw === 0) {
      decoded.latitude = 0;
      decoded.longitude = 0;
      decoded.valid = false;
    } else {
      decoded.latitude = latRaw / 100000.0;
      decoded.longitude = lonRaw / 100000.0;
      decoded.valid = true;
    }

    return {data: decoded};
  }

  // Jeśli przyszedł nieznany format danych
  return {data: {error: 'Nieznany format payloadu'}};
}

// Kodowanie wiadomości DO urządzenia (ChirpStack -> T-Beam)
function encodeDownlink(input) {
  // Jeśli z panelu lub skryptu wpadnie {"command": "TEKST"}
  if (input.data.command) {
    let str = input.data.command;
    let bytes = [];

    // Konwertujemy tekst na tablicę bajtów ASCII
    for (let i = 0; i < str.length; i++) {
      bytes.push(str.charCodeAt(i));
    }

    // Zwracamy na domyślnym porcie fPort = 1
    return {bytes: bytes, fPort: 1};
  }

  return {bytes: []};
}