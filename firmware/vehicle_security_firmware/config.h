#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// =============================================================================
// KONFIGURASI IDENTITAS PERANGKAT
// =============================================================================
#define DEVICE_ID           "vehicle_01"
#define FIRMWARE_VERSION    "v2.4.0-PRO"
#define VEHICLE_NAME        "Yamaha Jupiter Z1 - Smart IoT"

// =============================================================================
// PEMETAAN PIN NODEMCU ESP8266 (Didelegasikan ke file pin_config.h)
// =============================================================================
#include "pin_config.h"

// =============================================================================
// KONFIGURASI JARINGAN & SERVER
// =============================================================================
// Koneksi WiFi (Utama untuk kecepatan sync real-time)
#define WIFI_SSID           "Cyclop2"
#define WIFI_PASSWORD       "Cyclop2004"

// Koneksi APN GSM/GPRS Telkomsel (Kartu As / SimPATI / Halo / By.U)
#define GSM_APN             "telkomsel"       // APN Resmi Telkomsel
#define GSM_APN_USER        "wg"              // Default user Telkomsel
#define GSM_APN_PASS        "wg"              // Default password Telkomsel

// Konfigurasi Firebase Realtime Database
// URL Firebase tanpa protokol 'https://' dan tanpa trailing slash '/'
#define FIREBASE_HOST       "motor-monitor-9f391-default-rtdb.asia-southeast1.firebasedatabase.app"
#define FIREBASE_AUTH       "AIzaSyCrzjFytXDgX2QQMfvq64rvltuDJPjlY-0"

// Nomor HP Pemilik Kendaraan untuk Notifikasi SMS Darurat
// Format internasional (misal: +6281234567890)
#define OWNER_PHONE_NUMBER  "+6285217421209"

// =============================================================================
// PARAMETER INTERVAL WAKTU & TIMING (MILLISECONDS)
// =============================================================================
#define INTERVAL_TELEMETRY_FAST   1500   // 1.5 detik; beri waktu SoftwareSerial GPS
#define INTERVAL_TELEMETRY_SLOW   3000   // 3 detik saat idle / parkir
#define INTERVAL_COMMAND_POLL     1200   // 1.2 detik agar HTTPS tidak menelan data NMEA
#define INTERVAL_GSM_CHECK        30000  // 30 detik cek sinyal GSM CSQ
#define INTERVAL_GPRS_SYNC        8000   // 8 detik saat mode GPRS aktif
#define INTERVAL_NETWORK_GUARD    75     // Jeda minimum antar transaksi jaringan

// Parameter Sensor Getar & Alarm Cerdas (Anti False-Alarm)
#define VIBRATION_DEBOUNCE_MS     120    // Filter bouncing antar pulsa getaran (ms)
#define VIBRATION_WINDOW_MS       3000   // Jendela waktu akumulasi getaran (3 detik)
#define VIBRATION_SHOCK_THRESHOLD 4      // Jumlah getaran beruntun untuk memicu Alarm & SMS (Misal: 4 kali getaran)
#define VIBRATION_ALARM_HOLD_MS   20000  // Durasi sirene berbunyi saat alarm terpicu (20 detik)
#define GEOFENCE_DEFAULT_RADIUS   20     // Radius geofence default dalam meter (20 meter)

#endif // CONFIG_H
