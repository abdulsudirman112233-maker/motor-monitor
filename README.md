# 🏍️ IoT Smart Motorcycle Security & Live GPS Tracking (Motor-Monitor)

Sistem Keamanan & Pelacak Kendaraan Cerdas berbasis **NodeMCU ESP8266**, **GPS NEO-6M**, **GSM SIM800L**, dan **Firebase Realtime Database** dengan antarmuka Web Dashboard modern real-time.

---

## ✨ Fitur Utama

- 🛰️ **Pelacakan GPS Real-Time**: Menampilkan posisi real-time kendaraan pada peta interaktif (Leaflet.js) dengan dukungan Citra Satelit (Esri), Peta Jalan (OpenStreetMap), Dark Mode, serta integrasi 1-klik ke **Google Maps**.
- 📡 **Dual-Mode Network Failover**: 
  - **WiFi Online**: Kecepatan transmisi ultra-cepat saat berada dalam jangkauan WiFi rumah/hotspot.
  - **GPRS SIM800L Fallback**: Otomatis beralih ke jaringan seluler GPRS saat di luar jangkauan WiFi.
- 📲 **Peringatan SMS Darurat Otomatis**:
  - Mengirim link Google Maps saat WiFi terputus.
  - Mengirim SMS peringatan saat terdeteksi guncangan/pencurian.
  - Tombol **KIRIM SMS SOS** dari Web Dashboard.
  - Mendukung kontrol SMS jarak jauh: `#KUNCI`, `#BUKA`, `#MATIKAN`, `#HIDUPKAN`, `#LOKASI`, `#STATUS`.
- 🛡️ **Sensor Getar Cerdas (Anti False-Alarm)**: Algoritma *Two-Stage Progressive Shock Filtering* (getaran ringan hanya membunyikan beep peringatan lokal; getaran keras beruntun $\ge 4$ kali dalam 3 detik memicu sirene alarm & SMS darurat).
- 🔌 **Engine Cut-Off (Relay)**: Mematikan pengapian mesin kendaraan dari jarak jauh melalui Web Dashboard atau SMS.
- 📊 **Monitoring Telemetri Lengkap**: Memantau kecepatan (Speedometer HUD), tegangan aki (Accu 12V), kualitas sinyal GSM (CSQ), jumlah satelit GPS, dan log riwayat kejadian.

---

## 📌 Konfigurasi Pin Wiring Hardware

| Modul Hardware | Pin Modul | Pin NodeMCU ESP8266 | GPIO / Keterangan |
| :--- | :--- | :--- | :--- |
| **GPS NEO-6M** | TX | **D5** | GPIO14 (SoftSerial RX) |
| | RX | **D6** | GPIO12 (SoftSerial TX) |
| | VCC / GND | 3.3V / GND | Catu daya modul GPS |
| **GSM SIM800L** | TXD | **D1** | GPIO05 (SoftSerial RX) |
| | RXD | **D2** | GPIO04 (SoftSerial TX) |
| | VCC / GND | Step-Down 3.8V - 4.0V | Dedicated LM2596 (Min 2A) + Elco 1000µF |
| **Sensor Getar SW-420** | DO (Digital Out) | **D7** | GPIO13 |
| | VCC / GND | 3.3V / GND | Catu daya sensor |
| **Modul Relay (Engine Kill)** | IN (Signal) | **D8** | GPIO15 (HIGH = Cut-Off / Putus Kontak) |
| | VCC / GND | 5V (Vin) / GND | Jalur pemutus pengapian CDI/Coil |
| **Buzzer / Sirene Alarm** | POS (+) | **D3** | GPIO00 (Active-HIGH) |
| | NEG (-) | GND | Buzzer alarm |
| **Sensor Tegangan Aki** | Signal | **A0** | ADC0 (Voltage Divider R1 100k + R2 10k) |

---

## 📂 Struktur Repositori

```text
GPSDANGSM/
├── dashboard/                     # Web Dashboard Front-End
│   ├── css/
│   │   └── style.css              # Glassmorphism Dark UI Styling
│   ├── js/
│   │   ├── app.js                 # Controller Utama Real-Time & Firebase Sync
│   │   ├── config.js              # Konfigurasi Firebase SDK & Map Layer
│   │   ├── map_controller.js      # Engine Peta Leaflet & Marker
│   │   ├── telemetry_viewer.js    # Speedometer HUD & Telemetry Cards
│   │   └── vehicle_controls.js    # Dispatcher Perintah Jarak Jauh
│   ├── index.html                 # Halaman Utama Dashboard
│   └── serve.ps1                  # Local HTTP Server Runner
├── firmware/
│   ├── vehicle_security_firmware/ # Firmware Utama NodeMCU ESP8266
│   │   ├── vehicle_security_firmware.ino
│   │   ├── config.h               # Pengaturan WiFi, APN, Firebase, Nomor HP
│   │   ├── pin_config.h           # Pengaturan Pinout Hardware
│   │   ├── actuators.h / .cpp     # Kontrol Relay & Buzzer
│   │   ├── firebase_client.h / .cpp # HTTP REST Client (WiFi & GPRS Failover)
│   │   ├── gps_manager.h / .cpp   # Parser NMEA TinyGPSPlus
│   │   ├── gsm_sim800l.h / .cpp   # SMS & GPRS HTTP Driver
│   │   └── security_system.h / .cpp # Logika Keamanan & Filter Sensor Getar
│   └── test_kirim_firebase/       # Sketch Uji Coba Minimalis Firebase
├── kirim_data_test.bat            # 1-Click Tool Uji Coba Pengiriman Data
├── kirim_data_test.ps1            # Script Pengirim Data Telemetri PowerShell
├── KONFIGURASI_PIN.txt            # Panduan Lengkap Rangkaian Pin
└── README.md
```

---

## 🚀 Panduan Memulai

### 1. Persiapan Firmware (Arduino IDE)
1. Buka Arduino IDE, pastikan board **NodeMCU 1.0 (ESP-12E Module)** telah terpasang.
2. Install library berikut melalui Library Manager:
   - `TinyGPSPlus`
   - `ArduinoJson` (v6.x)
   - `SoftwareSerial`
3. Buka file [firmware/vehicle_security_firmware/config.h](file:///d:/GPSDANGSM/firmware/vehicle_security_firmware/config.h) dan sesuaikan:
   - `WIFI_SSID` & `WIFI_PASSWORD`
   - `GSM_APN` (misal: `"internet"`)
   - `OWNER_PHONE_NUMBER` (Nomor HP pemilik, format: `+628...`)
4. Hubungkan NodeMCU ke komputer melalui kabel USB dan klik **Upload**.

### 2. Menjalankan Web Dashboard
1. Jalankan server lokal dengan PowerShell di folder `dashboard/`:
   ```powershell
   powershell -ExecutionPolicy Bypass -File "dashboard\serve.ps1"
   ```
2. Buka browser dan akses: **`http://localhost:3000`**.

---

## 📄 Lisensi
Proyek ini dibuat untuk keperluan monitoring & sistem keamanan kendaraan cerdas berbasis IoT.
