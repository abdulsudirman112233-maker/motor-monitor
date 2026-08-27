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
uint32_t lastNetworkOperationTime = 0;
uint32_t lastProcessedCommandTime = 0;
bool telemetryPushRequested = true;
bool engineStateSyncRequested = false;
bool lastObservedEngineLock = false;
uint32_t lastEngineSyncAttemptTime = 0;

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
    else if (cmd.startsWith("#JARAK") || cmd.startsWith("#RADIUS") || cmd.startsWith("#GEOFENCE")) {
        int spaceIdx = cmd.indexOf(' ');
        if (spaceIdx != -1) {
            String valStr = cmd.substring(spaceIdx + 1);
            valStr.trim();
            float newRadius = valStr.toFloat();
            if (newRadius >= 5.0f && newRadius <= 10000.0f) {
                securitySystem.setGeofenceRadius(newRadius);
                securitySystem.saveGeofenceToEEPROM();
                reply = "[IoT GEOFENCE] Batas radius aman disetel ke: " + String(newRadius, 0) + " Meter (Tersimpan di EEPROM).";
                firebaseClient.pushLogEvent("SMS_GEOFENCE", "Batas radius geofence diubah via SMS menjadi " + String(newRadius, 0) + "m", gpsManager.getLatitude(), gpsManager.getLongitude(), gpsManager.getSpeed());
            } else {
                reply = "[IoT GEOFENCE] Nilai radius tidak valid (5 - 10000 meter). Contoh: #JARAK 50";
            }
        } else {
            reply = "[IoT GEOFENCE] Format: #JARAK <meter>. Contoh: #JARAK 50";
        }
    } 
    else if (cmd == "#PAGAR ON" || cmd == "#GEOFENCE ON") {
        securitySystem.setGeofenceEnabled(true);
        securitySystem.saveGeofenceToEEPROM();
        reply = "[IoT GEOFENCE] Pembatas Jarak / Geofence DIAKTIFKAN (Tersimpan lokal).";
    }
    else if (cmd == "#PAGAR OFF" || cmd == "#GEOFENCE OFF") {
        securitySystem.setGeofenceEnabled(false);
        securitySystem.saveGeofenceToEEPROM();
        reply = "[IoT GEOFENCE] Pembatas Jarak / Geofence DINONAKTIFKAN.";
    }
    else if (cmd == "#TITIK" || cmd == "#SETANCHOR" || cmd == "#PARKIR") {
        if (gpsManager.hasValidFix()) {
            double lat = gpsManager.getLatitude();
            double lng = gpsManager.getLongitude();
            securitySystem.setAnchorPoint(lat, lng);
            securitySystem.setGeofenceEnabled(true);
            securitySystem.saveGeofenceToEEPROM();
            reply = "[IoT GEOFENCE] Titik pusat geofence berhasil disetel ke posisi GPS saat ini (" + String(lat, 5) + ", " + String(lng, 5) + "). Pembatas jarak AKTIF.";
        } else {
            reply = "[IoT GEOFENCE] Gagal menyetel titik parkir: GPS belum menemukan sinyal satelit (No Fix).";
        }
    }
    else {
        reply = "[IoT KENDARAAN] Perintah tidak dikenali. Ketik format:\n#KUNCI / #BUKA\n#MATIKAN / #HIDUPKAN\n#PAGAR ON / #PAGAR OFF\n#TITIK (set parkir)\n#JARAK <meter>\n#LOKASI / #STATUS";
    }

    if (reply.length() > 0) {
        gsmManager.sendSMS(sender, reply, "SMS_REPLY");
    }
}

// Fungsi memproses perintah jarak jauh dari Firebase Web Dashboard
bool processWebControls() {
    ControlCommands cmds;
    if (firebaseClient.fetchControlCommands(cmds)) {
        const bool isNewCommand = cmds.lastCommandTime != 0 &&
                                  cmds.lastCommandTime != lastProcessedCommandTime;
        bool geofenceChanged = false;

        // 1. Kontrol Relay Pemutus Mesin
        if (isNewCommand && cmds.lockEngine != actuatorManager.isEngineLocked()) {
            Serial.print(F("[WEB CONTROL] Perintah Engine Lock Berubah ke: "));
            Serial.println(cmds.lockEngine ? F("LOCKED (D0 LOW / NC OPEN)") : F("UNLOCKED (D0 HIGH / NC CLOSED)"));
            actuatorManager.setEngineLocked(cmds.lockEngine);
            String logMsg = cmds.lockEngine ? "Engine Cut-off diaktifkan dari Web Dashboard" : "Engine Cut-off dinonaktifkan dari Web Dashboard";
            firebaseClient.pushLogEvent("WEB_CONTROL", logMsg, gpsManager.getLatitude(), gpsManager.getLongitude(), gpsManager.getSpeed());
        }

        // 2. Kontrol Mode Keamanan (Arm/Disarm)
        if (isNewCommand && cmds.armed && !securitySystem.isArmed()) {
            securitySystem.arm();
            firebaseClient.pushLogEvent("WEB_CONTROL", "Sistem di-ARM dari Web Dashboard", gpsManager.getLatitude(), gpsManager.getLongitude(), gpsManager.getSpeed());
        } else if (isNewCommand && !cmds.armed && securitySystem.isArmed()) {
            securitySystem.disarm();
            firebaseClient.pushLogEvent("WEB_CONTROL", "Sistem di-DISARM dari Web Dashboard", gpsManager.getLatitude(), gpsManager.getLongitude(), gpsManager.getSpeed());
        }

        // 3. Tombol Sirene Panic Alarm dari Web Dashboard
        if (isNewCommand && cmds.triggerPanic) {
            Serial.println(F("[WEB CONTROL] >>> PANIC SIREN: MENGAKTIFKAN BUZZER PADA PIN D8! <<<"));
            actuatorManager.triggerPanicSiren();
            securitySystem.triggerAlarm("WEB_DASHBOARD_PANIC_BUTTON");
            firebaseClient.acknowledgeCommand("trigger_panic");
            firebaseClient.pushLogEvent("PANIC_SIREN", "Tombol Panic Siren Ditekan dari Web - Buzzer Aktif", gpsManager.getLatitude(), gpsManager.getLongitude(), gpsManager.getSpeed());
        }

        // 4. Tombol Cari Kendaraan (Chirp)
        if (isNewCommand && cmds.findVehicle) {
            Serial.println(F("[WEB CONTROL] Tombol Cari Motor Ditekan -> 3x Beep Buzzer"));
            actuatorManager.triggerFinderChirp();
            firebaseClient.acknowledgeCommand("find_vehicle");
        }

        // 5. Reset Alarm & Matikan Buzzer
        if (isNewCommand && cmds.resetAlarm) {
            Serial.println(F("[WEB CONTROL] Reset Alarm Ditekan -> Mematikan Buzzer"));
            actuatorManager.stopBuzzer();
            securitySystem.resetAlarm();
            firebaseClient.acknowledgeCommand("reset_alarm");
        }

        // 6. Request Kirim SMS Darurat Manual dari Web Dashboard
        if (isNewCommand && cmds.emergencySmsRequest) {
            Serial.println(F("[WEB CONTROL] Tombol 'KIRIM SMS SOS' Ditekan dari Web Dashboard!"));
            
            double lat = gpsManager.getLatitude();
            double lng = gpsManager.getLongitude();
            String mapsUrl = (lat != 0.0 && lng != 0.0) ? 
                ("https://maps.google.com/?q=" + String(lat, 6) + "," + String(lng, 6)) : 
                "Mencari Sinyal GPS";

            String sosMsg = "[SOS DARURAT IOT]\nLokasi: " + mapsUrl + "\nKecepatan: " + String(gpsManager.getSpeed(), 1) + " km/h\nAki: " + String(readBatteryVoltage(), 1) + "V";
            
            bool sent = gsmManager.sendSMS(OWNER_PHONE_NUMBER, sosMsg, "SOS");
            if (!sent && String(OWNER_PHONE_NUMBER).startsWith("+62")) {
                // Fallback jika format +62 ditolak operator: Coba format lokal 08xxx
                String localNumber = "0" + String(OWNER_PHONE_NUMBER).substring(3);
                Serial.print(F("[WEB CONTROL] Mencoba fallback format nomor lokal: "));
                Serial.println(localNumber);
                sent = gsmManager.sendSMS(localNumber, sosMsg, "SOS");
            }

            if (sent) {
                Serial.println(F("[WEB CONTROL] SOS SMS Sukses Terkirim ke No Pemilik."));
                firebaseClient.pushLogEvent("SOS_SMS_SENT", "SMS darurat sukses terkirim ke pemilik (" OWNER_PHONE_NUMBER ")", lat, lng, gpsManager.getSpeed());
            } else {
                Serial.println(F("[WEB CONTROL] Gagal Kirim SMS SOS. Cek sinyal & pulsa SIM800L."));
                firebaseClient.pushLogEvent("SOS_SMS_FAILED", "Gagal kirim SMS: Cek pulsa aktif atau catu daya SIM800L", lat, lng, gpsManager.getSpeed());
            }
            firebaseClient.acknowledgeCommand("emergency_sms_request");
        }

        // 7. ======= SINKRONISASI PENUH GEOFENCE DARI WEB DASHBOARD =======

        // 7a. Update Batas Radius Geofence Dinamis
        if (isNewCommand && cmds.geofenceRadius > 0 && cmds.geofenceRadius != securitySystem.getGeofenceRadius()) {
            Serial.print(F("[WEB SYNC] Radius Geofence diubah ke: "));
            Serial.print(cmds.geofenceRadius);
            Serial.println(F(" Meter"));
            securitySystem.setGeofenceRadius(cmds.geofenceRadius);
            geofenceChanged = true;
        }

        // 7b. Sinkronkan Toggle Geofence Aktif/Nonaktif
        if (isNewCommand && cmds.geofenceEnabled != securitySystem.isGeofenceActive()) {
            Serial.print(F("[WEB SYNC] Geofence Enabled diubah ke: "));
            Serial.println(cmds.geofenceEnabled ? F("AKTIF") : F("NONAKTIF"));
            securitySystem.setGeofenceEnabled(cmds.geofenceEnabled);
            geofenceChanged = true;
            firebaseClient.pushLogEvent("WEB_CONTROL",
                cmds.geofenceEnabled ? "Geofence diaktifkan dari Web" : "Geofence dinonaktifkan dari Web",
                gpsManager.getLatitude(), gpsManager.getLongitude(), gpsManager.getSpeed());
        }

        // 7c. Sinkronkan Auto Cut-Off saat Keluar Radius
        // Jangan menerapkan ulang nilai auto-cutoff lama ketika tombol lain
        // ditekan. Hanya perintah toggle auto-cutoff yang boleh mengubahnya.
        if (isNewCommand && cmds.lastCommandKey == "auto_cutoff_geofence" &&
            cmds.autoCutoffGeofence != securitySystem.isAutoCutoffGeofence()) {
            Serial.print(F("[WEB SYNC] Auto Cut-Off Geofence diubah ke: "));
            Serial.println(cmds.autoCutoffGeofence ? F("AKTIF") : F("NONAKTIF"));
            securitySystem.setAutoCutoffGeofence(cmds.autoCutoffGeofence);
            geofenceChanged = true;
        }

        // 7d. Sinkronkan Titik Anchor (Parkir) dari Web Dashboard
        if (isNewCommand && cmds.anchorLat != 0.0 && cmds.anchorLng != 0.0) {
            double currentAnchorLat = securitySystem.getAnchorLatitude();
            double currentAnchorLng = securitySystem.getAnchorLongitude();
            double anchorDist = securitySystem.calculateDistanceMeters(currentAnchorLat, currentAnchorLng, cmds.anchorLat, cmds.anchorLng);
            if (anchorDist > 1.0 || currentAnchorLat == 0.0) {
                Serial.print(F("[WEB SYNC] Titik Anchor Geofence diperbarui dari Web: "));
                Serial.print(cmds.anchorLat, 6);
                Serial.print(F(", "));
                Serial.println(cmds.anchorLng, 6);
                securitySystem.setAnchorPoint(cmds.anchorLat, cmds.anchorLng);
                geofenceChanged = true;
                firebaseClient.pushLogEvent("WEB_CONTROL",
                    "Titik parkir geofence diset dari Web Dashboard",
                    cmds.anchorLat, cmds.anchorLng, gpsManager.getSpeed());
            }
        }

        // Simpan ke EEPROM jika ada perubahan geofence dari web
        if (geofenceChanged) {
            securitySystem.saveGeofenceToEEPROM();
        }

        if (isNewCommand) {
            lastProcessedCommandTime = cmds.lastCommandTime;
        }
        return isNewCommand || geofenceChanged;
    }
    return false;
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
    Serial.print(F("Reset reason  : "));
    Serial.println(ESP.getResetReason());
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

    // 1.1 Evaluasi Pagar Virtual Geofence secara real-time
    if (gpsManager.hasValidFix()) {
        securitySystem.autoArmGeofenceOnFix(gpsManager.getLatitude(), gpsManager.getLongitude());
    }
    securitySystem.checkGeofence(gpsManager.getLatitude(), gpsManager.getLongitude(), gpsManager.hasValidFix());

    // Tangkap semua perubahan relay, termasuk dari geofence dan SMS. Nilainya
    // akan disinkronkan ke cloud oleh scheduler tanpa mengubah ID command.
    const bool currentEngineLock = actuatorManager.isEngineLocked();
    if (currentEngineLock != lastObservedEngineLock) {
        lastObservedEngineLock = currentEngineLock;
        engineStateSyncRequested = true;
        telemetryPushRequested = true;
    }

    // 2. Deteksi perubahan status koneksi WiFi (Rumah / Hotspot HP) & Failover GPRS SIM800L
    bool currentWiFiState = firebaseClient.isWiFiConnected();
    if (lastWiFiState && !currentWiFiState) {
        // WiFi / Hotspot terputus
        wifiDisconnectTime = millis();
        Serial.println(F("[FAILOVER] WiFi/Hotspot Terputus! Mengaktifkan GPRS seluler SIM800L..."));
        gsmManager.initGPRS();
    } else if (!lastWiFiState && currentWiFiState) {
        // WiFi / Hotspot kembali terhubung
        Serial.println(F("[NETWORK] WiFi/Hotspot Terhubung Kembali."));
        wifiOfflineSmsSent = false;
    }

    // Jika WiFi terputus lebih dari 15 detik dan sistem dalam keadaan ARMED, kirim SMS notifikasi 1x
    if (!currentWiFiState && !wifiOfflineSmsSent && (millis() - wifiDisconnectTime > 15000) && wifiDisconnectTime > 0) {
        String mapsUrl = gpsManager.hasValidFix() ? gpsManager.getGoogleMapsLink() : "Mencari sinyal GPS";
        String alertMsg = "[IoT KENDARAAN]\nWiFi Terputus. Sistem beralih ke jaringan seluler SIM800L.\nLokasi: " + mapsUrl + "\nAki: " + String(readBatteryVoltage(), 1) + "V";
        gsmManager.sendSMS(OWNER_PHONE_NUMBER, alertMsg, "NETWORK");
        wifiOfflineSmsSent = true;
    }
    lastWiFiState = currentWiFiState;

    // 3. Cek apakah ada SMS masuk dari pemilik
    processIncomingSMS();

    // 4. Scheduler jaringan berbasis millis. Maksimal satu transaksi cloud per
    // putaran agar GET command dan PATCH telemetry tidak saling menumpuk.
    uint32_t cmdInterval = currentWiFiState ? INTERVAL_COMMAND_POLL : 8000;
    uint32_t currentInterval;
    if (currentWiFiState) {
        currentInterval = (gpsManager.getSpeed() > 1.0f || securitySystem.isAlarmActive()) ?
                           INTERVAL_TELEMETRY_FAST : INTERVAL_TELEMETRY_SLOW;
    } else {
        currentInterval = INTERVAL_GPRS_SYNC; // 8 detik saat mode GPRS
    }

    const uint32_t now = millis();
    const bool commandDue = (uint32_t)(now - lastCommandPollTime) >= cmdInterval;
    const bool telemetryDue = telemetryPushRequested ||
                              (uint32_t)(now - lastTelemetryPushTime) >= currentInterval;
    const bool networkReady = (uint32_t)(now - lastNetworkOperationTime) >= INTERVAL_NETWORK_GUARD;
    const uint32_t engineSyncRetryInterval = currentWiFiState ? 500 : INTERVAL_GPRS_SYNC;
    const bool engineSyncDue = engineStateSyncRequested &&
                               (uint32_t)(now - lastEngineSyncAttemptTime) >= engineSyncRetryInterval;

    // Command diprioritaskan. Bila ada perubahan, minta status ACK untuk dikirim
    // pada putaran berikutnya tanpa menunggu interval telemetry reguler.
    if (networkReady && engineSyncDue) {
        if (currentWiFiState) gpsManager.listen();
        const bool synced = firebaseClient.syncEngineLockState(lastObservedEngineLock);
        engineStateSyncRequested = !synced;
        lastEngineSyncAttemptTime = millis();
        lastNetworkOperationTime = lastEngineSyncAttemptTime;
    } else if (networkReady && commandDue) {
        if (currentWiFiState) gpsManager.listen();
        if (processWebControls()) {
            telemetryPushRequested = true;
        }
        lastCommandPollTime = millis();
        lastNetworkOperationTime = lastCommandPollTime;
    } else if (networkReady && telemetryDue) {
        if (currentWiFiState) gpsManager.listen();
        float vAki = readBatteryVoltage();
        GPSData gpsData = gpsManager.getData();
        GSMStatus gsmData = gsmManager.getStatus();

        bool pushOk = firebaseClient.pushTelemetry(gpsData, gsmData, securitySystem, actuatorManager, vAki);

        if (pushOk) {
            telemetryPushRequested = false;
            Serial.print(F("[TELEMETRY PUSH OK ("));
            Serial.print(currentWiFiState ? F("WIFI/HOTSPOT") : F("GPRS"));
            Serial.print(F(")] Lat: "));
            Serial.print(gpsData.latitude, 5);
            Serial.print(F(" | Lng: "));
            Serial.print(gpsData.longitude, 5);
            Serial.print(F(" | Speed: "));
            Serial.print(gpsData.speedKmh, 1);
            Serial.print(F(" km/h | Mode: "));
            Serial.println(securitySystem.getStateString());
        } else {
            // Hindari retry rapat yang membekukan loop; scheduler akan mencoba
            // lagi pada interval reguler berikutnya.
            telemetryPushRequested = false;
        }
        lastTelemetryPushTime = millis();
        lastNetworkOperationTime = lastTelemetryPushTime;
    }

    // 6. Log status berkala di Serial Monitor (setiap 10 detik)
    if (millis() - lastDebugPrintTime >= 10000) {
        lastDebugPrintTime = millis();
        Serial.print(F("[HEARTBEAT] Uptime: "));
        Serial.print(millis() / 1000);
        Serial.print(F("s | Internet: "));
        Serial.print(currentWiFiState ? F("WIFI / HOTSPOT CONNECTED") : F("OFFLINE (GPRS MODE)"));
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
