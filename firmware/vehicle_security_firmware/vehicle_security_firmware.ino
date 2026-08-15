/*
 * =====================================================================================
 * SISTEM KEAMANAN & PELACAK KENDARAAN REAL-TIME IOT
 * Platform    : NodeMCU ESP8266 (ESP-12E)
 * Modul       : GPS Neo-6M, GSM/GPRS SIM800L, Sensor SW-420, Relay 1-Ch, Buzzer 5V
 * Firmware    : v2.4.0-PRO
 * =====================================================================================
 */

#include <Arduino.h>
#include "config.h"
#include "gps_manager.h"
#include "gsm_sim800l.h"
#include "actuators.h"
#include "security_system.h"
#include "firebase_client.h"

// Inisialisasi Objek Modul
GPSManager          gpsManager(PIN_GPS_RX, PIN_GPS_TX);
GSMSim800L          gsmManager(PIN_GSM_RX, PIN_GSM_TX);
ActuatorManager     actuatorManager(PIN_RELAY_IGNITION, PIN_BUZZER);
SecuritySystem      securitySystem(PIN_SW420);
FirebaseSyncClient  firebaseClient;

// Timer millisecond non-blocking
uint32_t lastTelemetryPushTime = 0;
uint32_t lastCommandPollTime   = 0;
uint32_t lastDebugPrintTime    = 0;

// Fungsi helper membaca tegangan aki
float readBatteryVoltage() {
    int raw = analogRead(PIN_VOLTAGE_ADC);
    // Pembagi tegangan: R1 = 100k, R2 = 22k -> Rasio = (100+22)/22 = 5.545
    // ESP8266 ADC: 0-1023 setara dengan 0-3.3V
    float vAdc = (raw / 1023.0f) * 3.3f;
    float vAccu = vAdc * 5.545f;
    if (vAccu < 1.0f) vAccu = 12.6f; // Nilai default jika pin ADC tidak dihubungkan
    return vAccu;
}

// Fungsi memproses perintah SMS yang masuk dari nomor pemilik
void processIncomingSMS() {
    if (!gsmManager.hasIncomingSMS()) return;

    SMSMessage sms = gsmManager.getLatestSMS();
    String sender = sms.senderNumber;
    String cmd = sms.messageText;
    cmd.trim();
    cmd.toUpperCase();

    Serial.print(F("[SMS ROUTER] Memproses Perintah: "));
    Serial.print(cmd);
    Serial.print(F(" dari: "));
    Serial.println(sender);

    String reply = "";

    if (cmd == "#KUNCI" || cmd == "#LOCK" || cmd == "#ARM") {
        securitySystem.arm();
        reply = "[IoT KENDARAAN] Sistem Keamanan Berhasil DIKUNCI (ARMED). Sensor getar aktif.";
        firebaseClient.pushLogEvent("SMS_COMMAND", "Sistem dikunci via SMS oleh " + sender, gpsManager.getLatitude(), gpsManager.getLongitude(), gpsManager.getSpeed());
    } 
    else if (cmd == "#BUKA" || cmd == "#UNLOCK" || cmd == "#DISARM") {
        securitySystem.disarm();
        reply = "[IoT KENDARAAN] Sistem Keamanan DIBUKA (DISARMED). Sensor getar nonaktif.";
        firebaseClient.pushLogEvent("SMS_COMMAND", "Sistem dibuka via SMS oleh " + sender, gpsManager.getLatitude(), gpsManager.getLongitude(), gpsManager.getSpeed());
    } 
    else if (cmd == "#MATIKAN" || cmd == "#CUTOFF" || cmd == "#KILL") {
        actuatorManager.setEngineLocked(true);
        reply = "[IoT KENDARAAN] PERINTAH DITERIMA: Jalur pengapian mesin TELAH DIPUTUS (Engine Cut-Off Aktif).";
        firebaseClient.pushLogEvent("SMS_COMMAND", "Mesin dimatikan via SMS oleh " + sender, gpsManager.getLatitude(), gpsManager.getLongitude(), gpsManager.getSpeed());
    } 
    else if (cmd == "#HIDUPKAN" || cmd == "#RESTORE" || cmd == "#START") {
        actuatorManager.setEngineLocked(false);
        reply = "[IoT KENDARAAN] Pengapian mesin DIRESTORE (Normal). Kendaraan dapat dihidupkan kembali.";
        firebaseClient.pushLogEvent("SMS_COMMAND", "Pengapian direstore via SMS oleh " + sender, gpsManager.getLatitude(), gpsManager.getLongitude(), gpsManager.getSpeed());
    } 
    else if (cmd == "#LOKASI" || cmd == "#GPS" || cmd == "#LOC") {
        reply = "[IoT KENDARAAN LOKASI]\n";
        if (gpsManager.hasValidFix()) {
            reply += "Peta: " + gpsManager.getGoogleMapsLink() + "\n";
            reply += "Kecepatan: " + String(gpsManager.getSpeed(), 1) + " km/h\n";
            reply += "Satelit: " + String(gpsManager.getSatellites()) + "\n";
            reply += "Arah: " + String(gpsManager.getHeading(), 0) + " deg";
        } else {
            reply += "GPS sedang mencari sinyal satelit (No Fix). Lokasi terakhir: Lat " + 
                     String(gpsManager.getLatitude(), 5) + ", Lng " + String(gpsManager.getLongitude(), 5);
        }
    } 
    else if (cmd == "#STATUS") {
        reply = "[IoT KENDARAAN STATUS]\n";
        reply += "Keamanan: " + securitySystem.getStateString() + "\n";
        reply += "Mesin: " + String(actuatorManager.isEngineLocked() ? "TERKUNCI" : "NORMAL") + "\n";
        reply += "Aki: " + String(readBatteryVoltage(), 1) + "V\n";
        reply += "GSM Sinyal: " + String(gsmManager.getStatus().signalPercent) + "%\n";
        reply += "GPS Fix: " + String(gpsManager.hasValidFix() ? "YA" : "TIDAK") + "\n";
        reply += "Lokasi: " + gpsManager.getGoogleMapsLink();
    } 
    else if (cmd == "#BUNYI" || cmd == "#CARI" || cmd == "#PANIC") {
        actuatorManager.triggerFinderChirp();
        reply = "[IoT KENDARAAN] Buzzer pencarian dibunyikan (3x Chirp).";
    } 
    else {
        reply = "[IoT KENDARAAN] Perintah tidak dikenali. Ketik format:\n#KUNCI\n#BUKA\n#MATIKAN\n#HIDUPKAN\n#LOKASI\n#STATUS\n#BUNYI";
    }

    if (reply.length() > 0) {
        gsmManager.sendSMS(sender, reply);
    }
}

// Fungsi memproses perintah jarak jauh dari Firebase Web Dashboard
void processWebControls() {
    ControlCommands cmds;
    if (firebaseClient.fetchControlCommands(cmds)) {
        // 1. Kontrol Relay Pemutus Mesin
        if (cmds.lockEngine != actuatorManager.isEngineLocked()) {
            actuatorManager.setEngineLocked(cmds.lockEngine);
            String logMsg = cmds.lockEngine ? "Engine Cut-off diaktifkan dari Web Dashboard" : "Engine Cut-off dinonaktifkan dari Web Dashboard";
            firebaseClient.pushLogEvent("WEB_CONTROL", logMsg, gpsManager.getLatitude(), gpsManager.getLongitude(), gpsManager.getSpeed());
        }

        // 2. Kontrol Mode Keamanan (Arm/Disarm)
        if (cmds.armed && !securitySystem.isArmed()) {
            securitySystem.arm();
            firebaseClient.pushLogEvent("WEB_CONTROL", "Sistem di-ARM dari Web Dashboard", gpsManager.getLatitude(), gpsManager.getLongitude(), gpsManager.getSpeed());
        } else if (!cmds.armed && securitySystem.isArmed()) {
            securitySystem.disarm();
            firebaseClient.pushLogEvent("WEB_CONTROL", "Sistem di-DISARM dari Web Dashboard", gpsManager.getLatitude(), gpsManager.getLongitude(), gpsManager.getSpeed());
        }

        // 3. Tombol Sirene Panic Alarm
        if (cmds.triggerPanic) {
            securitySystem.triggerAlarm("WEB_DASHBOARD_PANIC_BUTTON");
            firebaseClient.acknowledgeCommand("trigger_panic");
        }

        // 4. Tombol Cari Kendaraan (Chirp)
        if (cmds.findVehicle) {
            actuatorManager.triggerFinderChirp();
            firebaseClient.acknowledgeCommand("find_vehicle");
        }

        // 5. Reset Alarm
        if (cmds.resetAlarm) {
            securitySystem.resetAlarm();
            firebaseClient.acknowledgeCommand("reset_alarm");
        }

        // 6. Request Kirim SMS Darurat Manual dari Web Dashboard
        if (cmds.emergencySmsRequest) {
            Serial.println(F("[WEB CONTROL] Tombol 'KIRIM SMS SOS' Ditekan dari Web Dashboard!"));
            
            double lat = gpsManager.getLatitude();
            double lng = gpsManager.getLongitude();
            String mapsUrl = (lat != 0.0 && lng != 0.0) ? 
                ("https://maps.google.com/?q=" + String(lat, 6) + "," + String(lng, 6)) : 
                "Mencari Sinyal GPS";

            String sosMsg = "[SOS DARURAT IOT]\nPermintaan lokasi dari Web Dashboard:\n" + mapsUrl + "\nKecepatan: " + String(gpsManager.getSpeed(), 1) + " km/h\nAki: " + String(readBatteryVoltage(), 1) + "V";
            
            bool sent = gsmManager.sendSMS(OWNER_PHONE_NUMBER, sosMsg);
            if (sent) {
                Serial.println(F("[WEB CONTROL] SOS SMS Sukses Terkirim ke No Pemilik."));
                firebaseClient.pushLogEvent("SOS_SMS_SENT", "SMS darurat sukses terkirim ke pemilik (" OWNER_PHONE_NUMBER ")", lat, lng, gpsManager.getSpeed());
            } else {
                Serial.println(F("[WEB CONTROL] Gagal Kirim SMS SOS. Cek sinyal & pulsa SIM800L."));
                firebaseClient.pushLogEvent("SOS_SMS_FAILED", "Gagal kirim SMS: Cek pulsa aktif atau catu daya SIM800L", lat, lng, gpsManager.getSpeed());
            }
            firebaseClient.acknowledgeCommand("emergency_sms_request");
        }
    }
}

// Status koneksi sebelumnya untuk deteksi perubahan jaringan
bool lastWiFiState = false;
uint32_t wifiDisconnectTime = 0;
bool wifiOfflineSmsSent = false;

void setup() {
    Serial.begin(115200);
    delay(500);

    Serial.println();
    Serial.println(F("================================================================="));
    Serial.println(F("      SISTEM KEAMANAN & PELACAK KENDARAAN IOT (ESP8266)         "));
    Serial.println(F("================================================================="));
    Serial.print(F("Device ID     : ")); Serial.println(DEVICE_ID);
    Serial.print(F("Firmware Vers : ")); Serial.println(FIRMWARE_VERSION);
    Serial.print(F("Vehicle Name  : ")); Serial.println(VEHICLE_NAME);
    Serial.println(F("-----------------------------------------------------------------"));

    // Inisialisasi Perangkat Keras
    actuatorManager.begin();
    gpsManager.begin(GPS_BAUD_RATE);
    gsmManager.begin(GSM_BAUD_RATE);
    securitySystem.begin(&actuatorManager, &gpsManager, &gsmManager);
    
    // Inisialisasi Koneksi Jaringan & Cloud (WiFi dengan Fallback GPRS SIM800L)
    firebaseClient.begin(&gsmManager);

    // Bunyikan nada startup pendek
    actuatorManager.triggerArmChirp();

    Serial.println(F("[SYSTEM] Sistem Berhasil Diinisialisasi. Memulai Loop Pengawasan..."));
    Serial.println(F("================================================================="));
}

void loop() {
    // 1. Update rutin seluruh modul
    gpsManager.update();
    gsmManager.update();
    actuatorManager.update();
    securitySystem.update();
    firebaseClient.updateWiFi();

    // 2. Deteksi perubahan status koneksi WiFi & SMS Failover Notifikasi
    bool currentWiFiState = firebaseClient.isWiFiConnected();
    if (lastWiFiState && !currentWiFiState) {
        // WiFi baru saja terputus
        wifiDisconnectTime = millis();
        Serial.println(F("[FAILOVER] WiFi Terputus! Mengaktifkan GPRS & mode seluler SIM800L..."));
        gsmManager.initGPRS();
    } else if (!lastWiFiState && currentWiFiState) {
        // WiFi kembali terhubung
        Serial.println(F("[NETWORK] WiFi Terhubung Kembali."));
        wifiOfflineSmsSent = false;
    }

    // Jika WiFi terputus lebih dari 15 detik dan sistem dalam keadaan ARMED / Siaga, kirim SMS notifikasi 1x
    if (!currentWiFiState && !wifiOfflineSmsSent && (millis() - wifiDisconnectTime > 15000) && wifiDisconnectTime > 0) {
        String mapsUrl = gpsManager.hasValidFix() ? gpsManager.getGoogleMapsLink() : "Mencari sinyal GPS";
        String alertMsg = "[IoT KENDARAAN]\nWiFi Terputus. Sistem beralih ke jaringan seluler SIM800L.\nLokasi: " + mapsUrl + "\nAki: " + String(readBatteryVoltage(), 1) + "V";
        gsmManager.sendSMS(OWNER_PHONE_NUMBER, alertMsg);
        wifiOfflineSmsSent = true;
    }
    lastWiFiState = currentWiFiState;

    // 3. Cek apakah ada SMS masuk dari pemilik
    processIncomingSMS();

    // 4. Polling perintah kontrol dari Web Dashboard (setiap 1.5 detik saat WiFi online, atau 8 detik saat GPRS)
    uint32_t cmdInterval = currentWiFiState ? INTERVAL_COMMAND_POLL : 8000;
    if (millis() - lastCommandPollTime >= cmdInterval) {
        lastCommandPollTime = millis();
        processWebControls();
    }

    // 5. Kirim data telemetri ke Firebase (Adaptif: 3s saat bergerak/alarm, 15s saat diam via WiFi; 8s via GPRS)
    uint32_t currentInterval;
    if (currentWiFiState) {
        currentInterval = (gpsManager.getSpeed() > 2.0 || securitySystem.isAlarmActive()) ? 
                           INTERVAL_TELEMETRY_FAST : INTERVAL_TELEMETRY_SLOW;
    } else {
        currentInterval = INTERVAL_GPRS_SYNC; // 8 detik saat mode GPRS
    }

    if (millis() - lastTelemetryPushTime >= currentInterval) {
        lastTelemetryPushTime = millis();
        
        float vAki = readBatteryVoltage();
        GPSData gpsData = gpsManager.getData();
        GSMStatus gsmData = gsmManager.getStatus();

        bool pushOk = firebaseClient.pushTelemetry(gpsData, gsmData, securitySystem, actuatorManager, vAki);

        if (pushOk) {
            Serial.print(F("[TELEMETRY PUSH OK ("));
            Serial.print(currentWiFiState ? F("WIFI") : F("GPRS"));
            Serial.print(F(")] Lat: "));
            Serial.print(gpsData.latitude, 5);
            Serial.print(F(" | Lng: "));
            Serial.print(gpsData.longitude, 5);
            Serial.print(F(" | Speed: "));
            Serial.print(gpsData.speedKmh, 1);
            Serial.print(F(" km/h | Mode: "));
            Serial.println(securitySystem.getStateString());
        }
    }

    // 6. Log status berkala di Serial Monitor (setiap 10 detik)
    if (millis() - lastDebugPrintTime >= 10000) {
        lastDebugPrintTime = millis();
        Serial.print(F("[HEARTBEAT] Uptime: "));
        Serial.print(millis() / 1000);
        Serial.print(F("s | WiFi: "));
        Serial.print(currentWiFiState ? F("CONNECTED") : F("OFFLINE (GPRS MODE)"));
        Serial.print(F(" | GSM CSQ: "));
        Serial.print(gsmManager.getStatus().csq);
        Serial.print(F(" ("));
        Serial.print(gsmManager.getStatus().signalPercent);
        Serial.print(F("%) | Sats: "));
        Serial.print(gpsManager.getSatellites());
        Serial.print(F(" | Engine: "));
        Serial.println(actuatorManager.isEngineLocked() ? F("LOCKED") : F("UNLOCKED"));
    }

    // Yield ke background task ESP8266 (WiFi & TCP Stack)
    yield();
}
