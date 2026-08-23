#ifndef PIN_CONFIG_H
#define PIN_CONFIG_H

#include <Arduino.h>

// =============================================================================
//               PANDUAN & SKEMA PENGKABELAN PIN NODEMCU ESP8266 (ESP-12E)
// =============================================================================
/*
 * +-----------+------------+---------------+--------------------------------------+
 * | PIN NODEMCU| GPIO ESP8266| PERANGKAT     | KETERANGAN & CARA SAMBUNG            |
 * +-----------+------------+---------------+--------------------------------------+
 * | D5        | GPIO 14    | GPS Neo-6M TX | Hubungkan D5 (ESP RX) ke TX Modul GPS|
 * | D6        | GPIO 12    | GPS Neo-6M RX | Hubungkan D6 (ESP TX) ke RX Modul GPS|
 * | D1        | GPIO 5     | SIM800L TXD   | Hubungkan D1 (ESP RX) ke TXD SIM800L |
 * | D2        | GPIO 4     | SIM800L RXD   | Hubungkan D2 (ESP TX) ke RXD SIM800L |
 * | D7        | GPIO 13    | Sensor SW-420 | Hubungkan D7 ke Pin DO (Digital Out) |
 * | D3        | GPIO 0     | Relay Modul   | Active LOW (Kontrol Pemutus CDI/Koil)|
 * | D0        | GPIO 16    | Buzzer Alarm  | Transistor Driver 2N2222 / Active 5V |
 * | A0        | ADC0       | Sensor Aki    | Pembagi Tegangan Resistor (Maks 3.3V)|
 * | 3V3       | 3.3V Power | VCC GPS / Sens| Output Regulator 3.3V NodeMCU        |
 * | VIN       | 5V Power   | Power Utama   | Input 5V dari Stepdown DC-DC LM2596  |
 * | GND       | Ground     | Common Ground | Sambungkan SEMUA GND perangkat jadi 1|
 * +-----------+------------+---------------+--------------------------------------+
 *
 * CATATAN PENTING SUMBER DAYA & KELISTRIKAN:
 * 1. Modul SIM800L membutuhkan arus sesaat hingga 2 Ampere pada tegangan 3.7V - 4.2V.
 *    Gunakan modul step-down Buck Converter LM2596 tersendiri yang diatur ke 4.0V
 *    dengan kapasitor 1000uF - 2200uF paralel pada pin VCC-GND SIM800L.
 * 2. Pin ADC A0 hanya menerima maksimal 3.3 Volt (1.0V internal via pembagi bawaan NodeMCU).
 *    Gunakan rangkaian Voltage Divider untuk Aki 12V:
 *    Aki (+) ---> Resistor R1 (100k Ohm) ---> Titik A0 ESP8266 ---> Resistor R2 (22k Ohm) ---> GND
 * 3. Sensor SW-420 sensitivitasnya dapat diatur lewat potensiometer pada modul sensor.
 * 4. Relay terhubung ke CDI / Switch Kontak motor (Normal Closed - NC untuk keamanan default).
 */

// =============================================================================
// 1. PIN MODUL GPS NEO-6M (SoftwareSerial)
// =============================================================================
// Pin RX ESP8266 menerima data NMEA kalimat ($GPRMC, $GPGGA) dari TX modul GPS
#define PIN_GPS_RX          D5    // GPIO14 - Terhubung ke pin TXD pada Modul GPS Neo-6M
// Pin TX ESP8266 mengirim konfigurasi (opsional) ke RX modul GPS
#define PIN_GPS_TX          D6    // GPIO12 - Terhubung ke pin RXD pada Modul GPS Neo-6M
#define GPS_BAUD_RATE       9600  // Baud rate standar komunikasi serial GPS Neo-6M

// =============================================================================
// 2. PIN MODUL GSM / GPRS SIM800L (SoftwareSerial)
// =============================================================================
// Pin RX ESP8266 menerima respon AT Commands & SMS dari TXD modul SIM800L
#define PIN_GSM_RX          D1    // GPIO5  - Terhubung ke pin TXD pada Modul SIM800L
// Pin TX ESP8266 mengirim AT Commands & teks SMS ke RXD modul SIM800L
#define PIN_GSM_TX          D2    // GPIO4  - Terhubung ke pin RXD pada Modul SIM800L
#define GSM_BAUD_RATE       9600  // Baud rate komunikasi serial GSM SIM800L

// =============================================================================
// 3. PIN SENSOR GETAR / VIBRATION SENSOR (SW-420)
// =============================================================================
// Input digital untuk mendeteksi gerakan / benturan / pembobolan saat sistem ARMED
#define PIN_SW420           D7    // GPIO13 - Input Digital dari Pin DO Sensor SW-420

// =============================================================================
// 4. PIN AKTUATOR MODUL RELAY 2-CHANNEL (ACTIVE LOW)
// =============================================================================
// Channel 1: Output digital untuk mematikan mesin jarak jauh (Engine Cut-Off)
// LOW = Relay ON (Memutus arus pengapian), HIGH = Relay OFF (Mesin normal)
#define PIN_RELAY_IGNITION  D3    // GPIO0  - Output Kontrol Relay Channel 1 (Engine Cut-Off)
#define PIN_RELAY_AUX       D4    // GPIO2  - Output Kontrol Relay Channel 2 (Cadangan / Klakson / Starter)

// =============================================================================
// 5. PIN AKTUATOR BUZZER ALARM (SIRENE & LOCATOR CHIRP)
// =============================================================================
// Output digital untuk membunyikan sirine alarm pencurian atau tanda suara locator
#define PIN_BUZZER          D0    // GPIO16 - Output Digital ke Driver Transistor Buzzer

// =============================================================================
// 6. PIN SENSOR TEGANGAN AKI MOTOR (VOLTAGE DIVIDER MONITORING)
// =============================================================================
// Input analog untuk memonitor kesehatan aki motor 12V secara real-time
#define PIN_VOLTAGE_ADC     A0    // ADC0   - Analog Input via Resistor Divider 100k/22k

#endif // PIN_CONFIG_H
