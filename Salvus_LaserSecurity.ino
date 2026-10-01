/*
  =====================================================================
  Salvus - Multi Zone Laser Security System
  =====================================================================
  Board        : ESP32 DevKit V1 (DOIT ESP32 DEVKIT V1)
  Sensors      : 3x LDR (laser-beam interruption, one per zone)
  Alerting     : Active buzzer + per-zone LED
  Notification : SIM800L GSM module (SMS alert)
  Timestamping : DS3231 RTC module (I2C)
  Logging      : MicroSD card module (SPI) -> log.csv

  Author       : <your name>
  Repository   : <your GitHub repo URL>
  License      : MIT

  -----------------------------------------------------------------
  WIRING (default pin map - change to match your build)
  -----------------------------------------------------------------
  LDR Zone 1        -> GPIO 34 (ADC1_CH6, input only)
  LDR Zone 2        -> GPIO 35 (ADC1_CH7, input only)
  LDR Zone 3        -> GPIO 32 (ADC1_CH4)

  Zone 1 LED        -> GPIO 25
  Zone 2 LED        -> GPIO 26
  Zone 3 LED        -> GPIO 27
  Buzzer            -> GPIO 33

  SIM800L  TX       -> GPIO 16 (ESP32 RX2)
  SIM800L  RX       -> GPIO 17 (ESP32 TX2)  (use a voltage divider / level
                       shifter, SIM800L RX is NOT 5V tolerant)
  SIM800L  VCC      -> external 4.0V regulated supply, 2A peak capable
  SIM800L  GND      -> common GND with ESP32

  DS3231 SDA        -> GPIO 21
  DS3231 SCL        -> GPIO 22
  DS3231 VCC/GND    -> 3.3V / GND

  MicroSD CS        -> GPIO 5
  MicroSD MOSI      -> GPIO 23
  MicroSD MISO      -> GPIO 19
  MicroSD SCK       -> GPIO 18
  MicroSD VCC/GND   -> 3.3V / GND (use a module with onboard regulator/
                       level shifting if using a raw SD card)

  -----------------------------------------------------------------
  REQUIRED LIBRARIES (install via Library Manager)
  -----------------------------------------------------------------
  - RTClib            by Adafruit      (DS3231 support)
  - SD                (bundled with ESP32 core)
  - SPI               (bundled with ESP32 core)
  - Wire              (bundled with ESP32 core)
  =====================================================================
*/

#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <RTClib.h>

// ---------------------------------------------------------------
// Pin configuration
// ---------------------------------------------------------------
const uint8_t LDR_PINS[3]  = {34, 35, 32};
const uint8_t LED_PINS[3]  = {25, 26, 27};
const uint8_t BUZZER_PIN   = 33;
const uint8_t SD_CS_PIN    = 5;

// ---------------------------------------------------------------
// GSM module (SIM800L) on Hardware Serial 2
// ---------------------------------------------------------------
HardwareSerial sim800(2);          // UART2
#define SIM800_RX_PIN 16           // ESP32 RX2 <- SIM800 TX
#define SIM800_TX_PIN 17           // ESP32 TX2 -> SIM800 RX
const char ALERT_PHONE_NUMBER[] = "+911234567890";   // <-- set your number

// ---------------------------------------------------------------
// RTC + SD globals
// ---------------------------------------------------------------
RTC_DS3231 rtc;
const char LOG_FILE[] = "/log.csv";

// ---------------------------------------------------------------
// Tunables
// ---------------------------------------------------------------
const int   LDR_THRESHOLD   = 2000;   // raw ADC value below which beam = "broken"
                                       // (tune to your laser/LDR pair; ESP32 ADC is 0-4095)
const unsigned long ZONE_DELAY_MS   = 500;
const unsigned long ALERT_HOLD_MS   = 4000;   // how long buzzer/LED stay on after a trigger

bool zoneTripped[3] = {false, false, false};

// =====================================================================
// SETUP
// =====================================================================
void setup() {
  Serial.begin(115200);
  delay(300);

  Serial.println();
  Serial.println("Salvus Multi Zone Laser Security System");
  Serial.println("System Initializing...");

  // --- GPIO init ---
  for (uint8_t i = 0; i < 3; i++) {
    pinMode(LED_PINS[i], OUTPUT);
    digitalWrite(LED_PINS[i], LOW);
  }
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
  Serial.println("ESP32 Initialized");

  // --- LDR / ADC init ---
  analogReadResolution(12);           // 0-4095
  for (uint8_t i = 0; i < 3; i++) {
    pinMode(LDR_PINS[i], INPUT);
  }
  Serial.println("LDR Sensors Initialized");

  // --- SIM800L init ---
  sim800.begin(9600, SERIAL_8N1, SIM800_RX_PIN, SIM800_TX_PIN);
  delay(2000);
  sim800.println("AT");              // basic handshake, response not blocking boot
  Serial.println("SIM800L Initialized");

  // --- DS3231 init ---
  Wire.begin();
  if (!rtc.begin()) {
    Serial.println("WARNING: DS3231 not found - check wiring");
  } else {
    if (rtc.lostPower()) {
      // Sets RTC to the sketch compile time if it lost power.
      // Comment this out after the first successful upload if you
      // don't want the RTC re-synced on every reset.
      rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    }
    Serial.println("DS3231 RTC Initialized");
  }

  // --- MicroSD init ---
  if (!SD.begin(SD_CS_PIN)) {
    Serial.println("WARNING: MicroSD init failed - check wiring/card");
  } else {
    if (!SD.exists(LOG_FILE)) {
      File f = SD.open(LOG_FILE, FILE_WRITE);
      if (f) {
        f.println("Date,Time,Zone,Event");
        f.close();
      }
    }
    Serial.println("MicroSD Initialized");
  }

  Serial.println("System Ready - Monitoring 3 Security Zones");
  Serial.println();
}

// =====================================================================
// MAIN LOOP
// =====================================================================
void loop() {
  for (uint8_t zone = 0; zone < 3; zone++) {
    Serial.printf("Monitoring Zone %d...\n", zone + 1);

    int reading = analogRead(LDR_PINS[zone]);

    if (reading < LDR_THRESHOLD && !zoneTripped[zone]) {
      triggerIntrusion(zone, reading);
    }

    delay(ZONE_DELAY_MS);
  }
}

// =====================================================================
// INTRUSION HANDLER
// =====================================================================
void triggerIntrusion(uint8_t zone, int rawValue) {
  zoneTripped[zone] = true;

  Serial.println("ALERT: Laser Beam Interrupted!");
  Serial.println("INTRUSION DETECTED");
  Serial.printf("Affected Zone: Zone %d\n", zone + 1);

  digitalWrite(BUZZER_PIN, HIGH);
  Serial.println("Buzzer: ON");

  digitalWrite(LED_PINS[zone], HIGH);
  Serial.printf("Zone %d LED: ON\n", zone + 1);

  // --- SMS alert ---
  Serial.println("Sending SMS Alert...");
  bool smsOk = sendSMSAlert(zone);
  Serial.println(smsOk ? "SMS Alert Sent" : "SMS Alert Failed");

  // --- Timestamp ---
  DateTime now = rtc.now();
  char dateStr[11];
  char timeStr[9];
  snprintf(dateStr, sizeof(dateStr), "%02d/%02d/%04d", now.day(), now.month(), now.year());
  snprintf(timeStr, sizeof(timeStr), "%02d:%02d:%02d", now.hour(), now.minute(), now.second());
  Serial.printf("Date: %s\n", dateStr);
  Serial.printf("Time: %s\n", timeStr);

  // --- Log to SD ---
  logEventToSD(dateStr, timeStr, zone);
  Serial.println("Event Logged to MicroSD");

  // --- Hold alert state, then reset ---
  delay(ALERT_HOLD_MS);
  digitalWrite(BUZZER_PIN, LOW);
  digitalWrite(LED_PINS[zone], LOW);
  zoneTripped[zone] = false;

  Serial.println("System returned to monitoring...");
  Serial.println();
}

// =====================================================================
// SIM800L - Send SMS
// =====================================================================
bool sendSMSAlert(uint8_t zone) {
  sim800.println("AT+CMGF=1");                 // text mode
  if (!waitForResponse("OK", 2000)) return false;

  sim800.print("AT+CMGS=\"");
  sim800.print(ALERT_PHONE_NUMBER);
  sim800.println("\"");
  if (!waitForResponse(">", 2000)) return false;

  sim800.print("Salvus Alert: Intrusion detected in Zone ");
  sim800.print(zone + 1);
  sim800.print(". Check premises immediately.");
  sim800.write(26);                             // Ctrl+Z sends the message

  return waitForResponse("OK", 8000);
}

// Helper: wait for a specific substring in the SIM800L response buffer
bool waitForResponse(const char* expected, unsigned long timeoutMs) {
  unsigned long start = millis();
  String buffer = "";
  while (millis() - start < timeoutMs) {
    while (sim800.available()) {
      buffer += (char)sim800.read();
      if (buffer.indexOf(expected) != -1) return true;
    }
  }
  return false;
}

// =====================================================================
// MicroSD - Append event row
// =====================================================================
void logEventToSD(const char* dateStr, const char* timeStr, uint8_t zone) {
  File f = SD.open(LOG_FILE, FILE_APPEND);
  if (f) {
    f.print(dateStr);
    f.print(",");
    f.print(timeStr);
    f.print(",Zone ");
    f.print(zone + 1);
    f.println(",Intrusion Detected");
    f.close();
  } else {
    Serial.println("WARNING: Could not write to log.csv");
  }
}
