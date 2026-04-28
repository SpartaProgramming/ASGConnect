// =========================================================
// DEKODER UPLINKÓW (Tag -> Serwer LNS -> Twój Backend)
// Tłumaczy tablicę bajtów z radia na obiekt JSON
// =========================================================
function decodeUplink(input) {
  var bytes = input.bytes;
  var decoded = {};

  if (bytes.length === 0) {
    return {errors: ['Pusty payload']};
  }

  var opCode = bytes[0];

  switch (opCode) {
    case 0x01:  // UL_TELEMETRY
      if (bytes.length >= 9) {
        decoded.type = 'TELEMETRY';

        // Odzyskiwanie 32-bitowych liczb całkowitych ze znakiem (Bit Shifting)
        var latInt =
            (bytes[1] << 24) | (bytes[2] << 16) | (bytes[3] << 8) | bytes[4];
        var lonInt =
            (bytes[5] << 24) | (bytes[6] << 16) | (bytes[7] << 8) | bytes[8];

        // Dzielenie przez milion, aby odzyskać wartości zmiennoprzecinkowe
        decoded.lat = latInt / 1000000.0;
        decoded.lon = lonInt / 1000000.0;
      } else {
        return {errors: ['Zla dlugosc ramki telemetrii']};
      }
      break;

    case 0x02:  // UL_KILLED
      decoded.type = 'KILLED';
      decoded.message = 'Gracz zostal wyeliminowany!';
      break;

    default:
      return {errors: ['Nieznany OpCode Uplinku: ' + opCode]};
  }

  return {data: decoded};
}

// =========================================================
// ENKODER DOWNLINKÓW (Twój Backend -> Serwer LNS -> Tag)
// Tłumaczy JSON wysłany z Twojego serwera na bajty dla Taga
// =========================================================
function encodeDownlink(input) {
  var data = input.data;
  var bytes = [];

  if (data.type === 'CONFIG') {
    bytes.push(0x01);  // DL_CONFIG
    bytes.push(data.team === 'BLUE' ? 1 : 0);
    bytes.push(data.gameType);

    // Czas z minut na 2 bajty (Big Endian)
    bytes.push((data.timeMins >> 8) & 0xFF);
    bytes.push(data.timeMins & 0xFF);

    bytes.push(data.alliesTotal);
    bytes.push(data.enemiesTotal);

    // Zamiana stringa (nicku) na tablicę bajtów (ASCII)
    for (var i = 0; i < data.nickname.length; i++) {
      bytes.push(data.nickname.charCodeAt(i));
    }
  } else if (data.type === 'COMMAND') {
    bytes.push(0x02);  // DL_COMMAND
    // Mapowanie stringów na bajty komend
    var cmdMap = {
      'START': 0x10,
      'END': 0x11,
      'WIN_BLUE': 0x12,
      'WIN_RED': 0x13,
      'YOU_DIED': 0x14
    };
    bytes.push(cmdMap[data.commandId]);
  } else if (data.type === 'UPDATE') {
    bytes.push(0x03);  // DL_UPDATE
    bytes.push(data.alliesAlive);
    bytes.push(data.enemiesAlive);
  }

  return {bytes: bytes, fPort: 1};
}