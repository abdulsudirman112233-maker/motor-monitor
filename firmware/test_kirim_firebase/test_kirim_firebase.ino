/*
 * =====================================================================================
 * SKETCH UJI COBA PENGIRIMAN DATA NODEMCU ESP8266 KE FIREBASE REALTIME DATABASE
 * Project: motor-monitor-9f391
 * =====================================================================================
 */

#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>

// 1. PENGATURAN WIFI
const char* WIFI_SSID     = "Cyclop";
const char* WIFI_PASSWORD = "Cyclop2000";

// 2. PENGATURAN FIREBASE (Region: Singapore / asia-southeast1)
const char* FIREBASE_HOST = "motor-monitor-9f391-default-rtdb.asia-southeast1.firebasedatabase.app";
const char* VEHICLE_ID    = "vehicle_01";

// Counter uji coba
int sendCounter = 0;

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    Serial.println();
    Serial.println(F("=========================================================="));
    Serial.println(F("  UJI COBA KIRIM DATA ESP8266 KE FIREBASE REALTIME DATABASE"));
    Serial.println(F("=========================================================="));
    
    // Sambungkan ke WiFi
    Serial.print(F("[WIFI] Menghubungkan ke: "));
    Serial.println(WIFI_SSID);
    
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(F("."));
    }
    
    Serial.println();
    Serial.print(F("[WIFI] Terhubung! IP Address: "));
    Serial.println(WiFi.localIP());
    Serial.println(F("=========================================================="));
}

void loop() {
    if (WiFi.status() == WL_CONNECTED) {
        sendCounter++;
        
        Serial.print(F("\n[KIRIM #"));
        Serial.print(sendCounter);
        Serial.println(F("] Menyiapkan payload JSON..."));
        
        // 1. Siapkan Dokumen JSON
        StaticJsonDocument<384> doc;
        doc["latitude"]           = -6.2088 + (sendCounter * 0.0001); // Koordinat bergerak maju sedikit
        doc["longitude"]          = 106.8456 + (sendCounter * 0.0001);
        doc["altitude"]           = 18.5;
        doc["speed"]              = 25.0 + (sendCounter % 30);        // Kecepatan variatif
        doc["heading"]            = 45.0;
        doc["satellites"]         = 8;
        doc["gps_fixed"]          = true;
        doc["battery_voltage"]    = 12.6;
        doc["power_source"]       = "ACCU_12V";
        doc["vibration_detected"] = false;
        doc["engine_running"]     = true;
        doc["connection_mode"]    = "WIFI_ONLINE";
        doc["timestamp"]          = millis() / 1000;
        
        char jsonBuffer[384];
        serializeJson(doc, jsonBuffer, sizeof(jsonBuffer));
        
        // 2. Kirim via HTTPS REST API ke Firebase
        WiFiClientSecure client;
        client.setInsecure(); // Bypass verifikasi sertifikat SSL
        client.setBufferSizes(512, 512);
        client.setTimeout(5000);
        
        HTTPClient http;
        String url = String("https://") + FIREBASE_HOST + "/vehicles/" + VEHICLE_ID + "/telemetry.json";
        
        if (http.begin(client, url)) {
            http.addHeader("Content-Type", "application/json");
            http.addHeader("Connection", "close");
            
            int httpCode = http.PATCH((uint8_t*)jsonBuffer, strlen(jsonBuffer));
            
            if (httpCode == 200 || httpCode == 204) {
                Serial.print(F(">>> [SUKSES 200 OK] Data berhasil terkirim ke Firebase: "));
                Serial.println(jsonBuffer);
            } else {
                Serial.print(F(">>> [GAGAL] HTTP Error Code: "));
                Serial.println(httpCode);
            }
            http.end();
        } else {
            Serial.println(F(">>> [GAGAL] Tidak dapat memulai koneksi HTTPS."));
        }
    } else {
        Serial.println(F("[WIFI] Terputus. Menghubungkan kembali..."));
        WiFi.reconnect();
    }
    
    // Kirim setiap 5 detik
    Serial.println(F("[INFO] Menunggu 5 detik untuk pengiriman berikutnya..."));
    delay(5000);
}
