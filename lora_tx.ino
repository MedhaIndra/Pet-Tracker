#include <TinyGPSPlus.h>
#include <RadioLib.h>

// ======================================================
//  Pin Configuration
// ======================================================
// GPS -> XIAO MG24
// GPS TX → D7 (RX1)
// GPS RX → D6 (TX1)
// VCC → 3.3V or VBUS
// GND → GND

// LoRa SX1278 -> XIAO MG24
// NSS (CS)   → D2
// DIO0       → D3
// RESET      → D4
// SCK        → D8
// MISO       → D9
// MOSI       → D10

// ======================================================
//  Objects
// ======================================================
TinyGPSPlus gps;
SX1278 radio = new Module(D2, D3, RADIOLIB_NC, RADIOLIB_NC);  // DIO1 not used

// ======================================================
//  Setup
// ======================================================
void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println("=================================");
  Serial.println("  GPS + LoRa Transmitter Started ");
  Serial.println("=================================");

  // --- GPS setup ---
  Serial1.begin(9600);   // UART1 for GPS
  Serial.println("GPS serial started on D7 (RX) and D6 (TX)");

  // --- LoRa setup ---
  int state = radio.begin(433.0);
  if (state == RADIOLIB_ERR_NONE) {
    Serial.println("LoRa radio initialized successfully!");
  } else {
    Serial.print("LoRa init failed, code: ");
    Serial.println(state);
    while (true);
  }

  Serial.println("Configured as LoRa TRANSMITTER");
  Serial.println("=================================\n");
}

// ======================================================
//  Loop
// ======================================================
void loop() {
  // --- Read from GPS continuously ---
  while (Serial1.available() > 0) {
    gps.encode(Serial1.read());
  }

  // --- Always print the latest GPS info ---
  if (millis() % 2000 < 50) {  // print every ~2 seconds
    Serial.println("------ Current GPS Data ------");

    if (gps.location.isValid()) {
      Serial.print("Latitude:  "); Serial.println(gps.location.lat(), 6);
      Serial.print("Longitude: "); Serial.println(gps.location.lng(), 6);
    } else {
      Serial.println("Location: Not yet fixed");
    }

    Serial.print("Satellites: ");
    Serial.println(gps.satellites.isValid() ? gps.satellites.value() : 0);
    Serial.print("HDOP: ");
    Serial.println(gps.hdop.isValid() ? gps.hdop.hdop() : 0);
    Serial.println("------------------------------\n");
  }

  // --- Transmit via LoRa only when GPS data is updated ---
  if (gps.location.isUpdated()) {
    double lat = gps.location.lat();
    double lng = gps.location.lng();
    int sats = gps.satellites.value();
    double hdop = gps.hdop.hdop();

    // Prepare message
    String message = String(lat, 6) + "," + String(lng, 6);


    Serial.print("Transmitting via LoRa: ");
    Serial.println(message);

    // Transmit
    int state = radio.transmit(message);
    if (state == RADIOLIB_ERR_NONE) {
      Serial.println("✅ LoRa Transmission successful!\n");
    } else {
      Serial.print("❌ LoRa transmit failed, code: ");
      Serial.println(state);
    }

    delay(1000);  // send every 2 seconds
  }
}