# Salvus – Multi Zone Laser Security System

An ESP32-based, 3-zone laser-beam intrusion detection system with SMS alerts,
RTC timestamping, and MicroSD event logging.

## Features
- 3 independent laser/LDR zones, scanned continuously
- Buzzer + per-zone LED indication on trip
- SMS alert via SIM800L GSM module
- Event timestamping via DS3231 RTC
- Event logging to a MicroSD card (`log.csv`)

## Hardware
| Component          | Qty |
|---------------------|-----|
| ESP32 DevKit V1      | 1   |
| Laser diode module   | 3   |
| LDR (photoresistor)  | 3   |
| 10kΩ resistor        | 3   |
| LED                  | 3   |
| Active buzzer        | 1   |
| SIM800L GSM module   | 1   |
| DS3231 RTC module    | 1   |
| MicroSD card module  | 1   |

## Wiring
See the pin map and notes at the top of `Salvus_LaserSecurity.ino`.

**Important:**
- SIM800L needs its own regulated 4.0V supply capable of ~2A peaks — do not
  power it from the ESP32's 3.3V/5V pins.
- SIM800L's RX pin is not 5V tolerant; use a voltage divider or logic-level
  shifter between ESP32 TX2 and SIM800L RX.
- GPIO 34 and 35 are input-only on the ESP32 and work well for the LDR
  dividers.

## Libraries required
Install these via the Arduino IDE Library Manager:
- `RTClib` by Adafruit (DS3231 support)
- `SD`, `SPI`, `Wire` — bundled with the ESP32 Arduino core

## Setup
1. Install the ESP32 board package in Arduino IDE (Boards Manager → "esp32").
2. Select **Tools → Board → DOIT ESP32 DEVKIT V1**.
3. Install the libraries listed above.
4. Open `Salvus_LaserSecurity.ino`, set `ALERT_PHONE_NUMBER` to your number.
5. Adjust `LDR_THRESHOLD` after testing your laser/LDR alignment — print raw
   `analogRead()` values first to find a good cutoff for your hardware.
6. Upload at 115200 baud and open the Serial Monitor at the same baud rate.

## Calibrating the LDR threshold
Laser/LDR pairs vary a lot by part and ambient light. Before relying on the
default `LDR_THRESHOLD`, add a temporary line in `loop()` to print
`analogRead(LDR_PINS[zone])` for each zone, observe the values with the beam
intact vs. interrupted, and set the threshold roughly halfway between them.

## License
MIT — see `LICENSE`.
