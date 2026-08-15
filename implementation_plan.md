# Rencana Implementasi: Sistem Keamanan & Pelacak Kendaraan Real-Time IoT (NodeMCU ESP8266 + GPS Neo-6M + SIM800L + Web Dashboard)

Rancangan sistem keamanan dan pelacakan kendaraan cerdas berbasis **NodeMCU ESP8266**, terintegrasi dengan modul **GPS Neo-6M**, modul GSM/GPRS **SIM800L**, sensor getar **SW-420**, **Relay 1-Channel** (pemutus pengapian mesin), **Buzzer Alarm 5V** (driver 2N2222), dan manajemen daya aki 12V (Dual Stepdown LM2596). Sistem ini dilengkapi **Dashboard Web Responsif Real-Time** dengan peta interaktif Leaflet.js, kontrol jarak jauh (Engine Kill Switch & Alarm), dan SMS darurat otomatis.

---

## 1. Arsitektur & Pemetaan Pin NodeMCU ESP8266

Karena NodeMCU ESP8266 hanya memiliki 1 port Hardware Serial UART utama (`Serial` pada TX/RX = GPIO1/GPIO3 yang digunakan untuk USB flashing & Debug Monitor), sistem menggunakan pustaka **`SoftwareSerial`** dengan alokasi pin khusus yang aman terhadap proses *booting* ESP8266.

### 1.1 Tabel Pengkabelan & Pinout ESP8266
| Komponen | Pin Modul | Pin NodeMCU ESP8266 | GPIO | Deskripsi & Fungsi |
| :--- | :--- | :--- | :--- | :--- |
| **GPS Neo-6M** | VCC | **5V (VIN)** | - | Catu daya GPS dari output 5V LM2596 |
| | GND | **GND** | - | Ground bersama |
| | TX | **D5** | **GPIO14** | `SoftwareSerial` RX (menerima NMEA dari GPS) |
| | RX | **D6** | **GPIO12** | `SoftwareSerial` TX (opsional konfigurasi GPS) |
| **SIM800L** | VCC | **4.0V (LM2596 #2)** | - | Wajib catu daya khusus 3.8V-4.2V min. 2A peak |
| | GND | **GND** | - | Ground bersama ESP8266 & Aki |
| | TXD | **D1** | **GPIO5** | `SoftwareSerial` RX (menerima data/respons AT) |
| | RXD | **D2** | **GPIO4** | `SoftwareSerial` TX (mengirim perintah AT) |
| **Sensor SW-420** | VCC | **3.3V** | - | Catu daya sensor 3.3V |
| | GND | **GND** | - | Ground |
| | DO | **D7** | **GPIO13** | Digital Input dengan Hardware Interrupt |
| **Buzzer 5V** | VCC (+) | **5V (VIN)** | - | Positif Buzzer |
| | GND (-) | **Kolektor 2N2222** | - | Emitor 2N2222 terhubung ke GND |
| | Kontrol | **D0** | **GPIO16** | Melalui Resistor 1kΩ ke Basis Transistor 2N2222 |
| **Relay 1-Ch** | VCC | **5V (VIN)** | - | Catu daya koil relay 5V |
| | GND | **GND** | - | Ground |
| | IN | **D3** | **GPIO0** | Trigger Relay Active LOW (Engine Cut-off) |
| | COM / NC | **Kabel Kontak/CDI** | - | Memutus jalur pengapian / starter motor |
| **Power Aki 12V** | Aki 12V | **Fuse 5A + Saklar** | - | Masuk ke LM2596 (5.0V) & LM2596 (4.0V) |

---

## 2. Struktur File Project Modular

Project disusun dalam direktori `C:\Users\Acer\.gemini\antigravity\scratch\vehicle_security_iot` dengan struktur modular siap pakai untuk **Antigravity IDE** dan **Arduino IDE**:

```
vehicle_security_iot/
│
├── README.md                           # Dokumentasi komprehensif, skematik wiring & panduan setup
├── SYSTEM_DESIGN.md                    # Detail teknis sistem, AT command SIM800L & API Firebase
│
├── firmware/                           # Program Arduino IDE Modular (NodeMCU ESP8266)
│   └── vehicle_security_firmware/
│       ├── vehicle_security_firmware.ino # Entry point: setup(), loop(), task timer scheduler
│       ├── config.h                    # Definisi Pin ESP8266, APN GPRS, Firebase URL, No. HP SMS
│       ├── gps_manager.h               # Header modul GPS (TinyGPS++)
│       ├── gps_manager.cpp             # Parsing koordinat, kecepatan, arah heading, validasi fix
│       ├── gsm_sim800l.h               # Header modul GSM SIM800L
│       ├── gsm_sim800l.cpp             # Engine AT Command (Kirim SMS, GPRS HTTP REST, cek sinyal)
│       ├── firebase_client.h           # Header sinkronisasi Firebase Realtime Database
│       ├── firebase_client.cpp         # Push data telemetri & fetch perintah kontrol (Relay/Buzzer)
│       ├── security_system.h           # Header State Machine keamanan (ARMED / DISARMED / ALARM)
│       ├── security_system.cpp         # Deteksi getaran SW-420, otomatis kirim SMS peringatan pencurian
│       ├── actuators.h                 # Header kontrol relay & buzzer
│       └── actuators.cpp               # Logika pemutus mesin & variasi nada buzzer (Chirp/Siren)
│
├── dashboard/                          # Web Dashboard Responsif Real-Time
│   ├── index.html                      # Antarmuka web modern & responsif (Desktop & Mobile)
│   ├── css/
│   │   └── style.css                   # Desain modern dark-mode, card telemetri, speedometer, animasi
│   └── js/
│       ├── config.js                   # Konfigurasi Firebase SDK & Endpoint RTDB
│       ├── map_controller.js           # Engine Peta Leaflet.js, marker kendaraan animasi, polyline trip
│       ├── telemetry_viewer.js         # Realtime Telemetry listener (Speed, Satelit, Sinyal, Baterai)
│       ├── vehicle_controls.js         # Pengontrol Relay (Engine Kill), Buzzer, Mode Arming
│       └── app.js                      # Controller utama, notifikasi alert, audio sirene di browser
│
└── database/
    ├── firebase_schema.json            # Template struktur JSON Firebase Realtime Database
    └── database.rules.json             # Security rules Firebase RTDB
```

---

## 3. Rincian Fitur & Mekanisme Kerja

### 3.1 Mode Keamanan & Deteksi Pencurian (Theft Protection)
1. **Mode ARMED (Terkunci)**:
   - Diaktifkan melalui Web Dashboard atau SMS (`#KUNCI`).
   - Jika sensor SW-420 mendeteksi getaran/gerakan motor:
     1. Buzzer berbunyi secara instan dengan pola sirene.
     2. Status `theft_alert: true` dikirim ke Firebase RTDB (Web Dashboard menampilkan modal darurat & alarm suara).
     3. Modul SIM800L otomatis mengirimkan SMS darurat berisi koordinat Google Maps (`https://maps.google.com/?q=-6.xxxx,106.xxxx`) ke nomor HP pemilik.
2. **Engine Cut-Off (Pemutus Mesin)**:
   - Pemilik dapat menekan tombol **"Matikan Mesin"** di Web Dashboard atau mengirim SMS `#MATIKAN`.
   - Relay D3 (GPIO0) akan terpicu untuk memutus jalur pengapian/starter motor sehingga kendaraan tidak bisa dihidupkan.
3. **Pencarian Kendaraan (Vehicle Finder / Panic Alarm)**:
   - Tombol **"Cari Kendaraan"** di Web Dashboard akan membunyikan buzzer sejenak (nada *chirp*) untuk menemukan posisi motor di tempat parkir.

### 3.2 Web Dashboard Responsif
- **Live GPS Tracking (Leaflet.js)**: Posisi kendaraan ter-update secara real-time tanpa perlu me-refresh halaman web, dilengkapi ikon motor yang berputar mengikuti arah pergerakan (*heading*).
- **HUD Telemetri**: Speedometer animasi, jumlah satelit GPS, kekuatan sinyal GSM (CSQ), status kontak mesin, dan status konektivitas perangkat.
- **Log Keamanan**: Riwayat waktu saat alarm terpicu, getaran terdeteksi, atau perintah kontrol dikirim.

---

## 4. Rencana Eksekusi
1. Membuat struktur direktori project di `C:\Users\Acer\.gemini\antigravity\scratch\vehicle_security_iot`.
2. Menulis seluruh kode firmware modular untuk Arduino IDE (`.ino`, `config.h`, `gps_manager`, `gsm_sim800l`, `firebase_client`, `security_system`, `actuators`).
3. Menulis seluruh kode Web Dashboard (HTML5, Modern CSS, Modular JS, Leaflet Map, Firebase RTDB integration).
4. Menyediakan file database schema dan rules Firebase.
5. Membuat dokumentasi arsitektur dan panduan kabel lengkap (`README.md` & `SYSTEM_DESIGN.md`).

---

## 5. Konfirmasi
Apakah rencana implementasi berbasis **NodeMCU ESP8266** ini sudah sesuai dan siap untuk saya buatkan seluruh file kodenya sekarang?
