#include "firebase_client.h"

FirebaseSyncClient::FirebaseSyncClient()
    : _gsm(nullptr), _host(FIREBASE_HOST), _auth(FIREBASE_AUTH), _deviceId(DEVICE_ID), _lastWiFiRetry(0), _lastGprsRetry(0) {
    // Sanitasi otomatis: Buang 'https://', 'http://', dan '/' di ujung URL jika ada
    if (_host.startsWith("https://")) _host = _host.substring(8);
    else if (_host.startsWith("http://")) _host = _host.substring(7);
    while (_host.endsWith("/")) _host = _host.substring(0, _host.length() - 1);
}

void FirebaseSyncClient::begin(GSMSim800L* gsm) {
    _gsm = gsm;

    Serial.print(F("[WIFI] Menghubungkan ke SSID (Rumah / Hotspot HP): "));
    Serial.println(WIFI_SSID);
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

void FirebaseSyncClient::updateWiFi() {
    if (WiFi.status() != WL_CONNECTED) {
        if (millis() - _lastWiFiRetry > 10000) {
            _lastWiFiRetry = millis();
            Serial.println(F("[WIFI] Mencoba menyambung kembali ke WiFi / Hotspot..."));
            WiFi.reconnect();
        }
    }
}

bool FirebaseSyncClient::isWiFiConnected() const {
    return WiFi.status() == WL_CONNECTED;
}

bool FirebaseSyncClient::isGprsActive() const {
    return _gsm && _gsm->isGprsConnected();
}

String FirebaseSyncClient::_buildUrl(const String &path) {
    String url = "https://" + _host + path;
    if (_auth.length() > 0 && 
        _auth != "YOUR_FIREBASE_DATABASE_SECRET_OR_WEB_API_KEY" && 
        !_auth.startsWith("AIzaSy")) {
        url += "?auth=" + _auth;
    }
    return url;
}

bool FirebaseSyncClient::_sendGprsRestRequest(const String &method, const String &path, const char* payload, String &responseOut) {
    if (!_gsm) return false;

    String fullUrl = _buildUrl(path);
    Serial.print(F("[GPRS FAILOVER] Mengirim REST "));
    Serial.print(method);
    Serial.println(F(" via SIM800L GPRS..."));

    if (method == "GET") {
        return _gsm->gprsHttpGet(fullUrl, responseOut);
    } else {
        String data = (payload != nullptr) ? String(payload) : "{}";
        return _gsm->gprsHttpPatch(fullUrl, data, responseOut);
    }
}

bool FirebaseSyncClient::_sendRestRequest(const String &method, const String &path, const char* payload, String &responseOut) {
    // 1. Coba via WiFi (Rumah / Hotspot HP) jika terhubung
    if (WiFi.status() == WL_CONNECTED) {
        WiFiClientSecure client;
        client.setInsecure(); // Bypass verifikasi sertifikat SSL untuk menghemat RAM
        client.setBufferSizes(512, 512);
        client.setTimeout(2500);

        HTTPClient http;
        String fullUrl = _buildUrl(path);

        if (http.begin(client, fullUrl)) {
            http.addHeader("Content-Type", "application/json");
            http.addHeader("Connection", "close");
            http.setTimeout(2500);
            http.setReuse(false);

            int httpCode = -1;
            if (method == "PATCH") {
                httpCode = (payload && strlen(payload) > 0) ? http.PATCH((uint8_t*)payload, strlen(payload)) : http.PATCH("");
            } else if (method == "PUT") {
                httpCode = (payload && strlen(payload) > 0) ? http.PUT((uint8_t*)payload, strlen(payload)) : http.PUT("");
            } else if (method == "POST") {
                httpCode = (payload && strlen(payload) > 0) ? http.POST((uint8_t*)payload, strlen(payload)) : http.POST("");
            } else if (method == "GET") {
                httpCode = http.GET();
            }

            if (httpCode == HTTP_CODE_OK || httpCode == 204) {
                responseOut = http.getString();
                http.end();
                return true;
            } else {
                Serial.print(F("[FIREBASE] HTTP Code: "));
                Serial.println(httpCode);
                http.end();
            }
        }
    }

    // 2. Fallback otomatis ke SIM800L GPRS jika WiFi / Hotspot tidak terhubung
    if (_gsm) {
        return _sendGprsRestRequest(method, path, payload, responseOut);
    }

    return false;
}

bool FirebaseSyncClient::pushTelemetry(const GPSData &gps, const GSMStatus &gsm, const SecuritySystem &sec, const ActuatorManager &act, float batteryVoltage) {
    // Validasi Koordinat: Gunakan titik real Baubau jika GPS belum menemukan fix
    double realLat = (gps.latitude != 0.0 && !isnan(gps.latitude)) ? gps.latitude : -5.460095;
    double realLng = (gps.longitude != 0.0 && !isnan(gps.longitude)) ? gps.longitude : 122.616677;

    // KINERJA TINGGI: Gabungkan Telemetri & Status dalam 1 Request HTTP PATCH Tunggal ke /vehicles/vehicle_01.json
    StaticJsonDocument<1024> doc;

    JsonObject telemetryObj = doc.createNestedObject("telemetry");
    telemetryObj["latitude"] = realLat;
    telemetryObj["longitude"] = realLng;
    telemetryObj["altitude"] = gps.altitude;
    telemetryObj["speed"] = gps.speedKmh;
    telemetryObj["heading"] = gps.heading;
    telemetryObj["satellites"] = gps.satellites;
    telemetryObj["hdop"] = gps.hdop;
    telemetryObj["gps_fixed"] = gps.isValid;
    telemetryObj["gsm_csq"] = gsm.csq;
    telemetryObj["gsm_signal_percent"] = gsm.signalPercent;
    telemetryObj["gsm_network"] = gsm.operatorName;
    telemetryObj["battery_voltage"] = batteryVoltage;
    telemetryObj["power_source"] = (batteryVoltage > 11.0) ? "ACCU_12V" : "LOW_BATTERY";
    telemetryObj["vibration_detected"] = sec.isVibrationDetected();
    telemetryObj["engine_running"] = (!act.isEngineLocked() && gps.speedKmh > 1.0f);
    telemetryObj["connection_mode"] = isWiFiConnected() ? "WIFI_ONLINE" : "GPRS_FALLBACK";

    JsonObject statusObj = doc.createNestedObject("status");
    statusObj["armed"] = sec.isArmed();
    statusObj["alarm_active"] = sec.isAlarmActive();
    statusObj["engine_locked"] = act.isEngineLocked();
    statusObj["relay_output_level"] = act.getRelayOutputLevel() == LOW ? "LOW" : "HIGH";
    statusObj["relay_pin"] = "D0/GPIO16";
    statusObj["sim800_ready"] = gsm.isModuleReady;
    statusObj["sim_registered"] = gsm.isSimRegistered;
    statusObj["sms_last_success"] = _gsm ? _gsm->wasLastSmsSuccessful() : false;
    statusObj["sms_last_type"] = _gsm ? _gsm->getLastSmsType() : "NONE";
    statusObj["sms_attempt_counter"] = _gsm ? _gsm->getSmsAttemptCounter() : 0;
    statusObj["theft_alert"] = sec.isAlarmActive();
    statusObj["last_alarm_reason"] = sec.getLastAlarmReason();
    statusObj["geofence_active"] = sec.isGeofenceActive();
    statusObj["geofence_radius_current"] = sec.getGeofenceRadius();
    statusObj["auto_cutoff_active"] = sec.isAutoCutoffGeofence();
    statusObj["anchor_lat_current"] = (sec.getAnchorLatitude() != 0.0) ? sec.getAnchorLatitude() : realLat;
    statusObj["anchor_lng_current"] = (sec.getAnchorLongitude() != 0.0) ? sec.getAnchorLongitude() : realLng;

    char payloadBuffer[1024];
    serializeJson(doc, payloadBuffer, sizeof(payloadBuffer));

    String path = "/vehicles/" + _deviceId + ".json";
    String response;
    return _sendRestRequest("PATCH", path, payloadBuffer, response);
}

bool FirebaseSyncClient::fetchControlCommands(ControlCommands &cmdsOut) {
    String path = "/vehicles/" + _deviceId + "/controls.json";
    String response;
    
    if (_sendRestRequest("GET", path, nullptr, response)) {
        StaticJsonDocument<384> doc;
        DeserializationError err = deserializeJson(doc, response);
        if (!err && !doc.isNull()) {
            cmdsOut.lockEngine = doc["lock_engine"] | false;
            cmdsOut.armed = doc["armed"] | false;
            cmdsOut.triggerPanic = doc["trigger_panic"] | false;
            cmdsOut.findVehicle = doc["find_vehicle"] | false;
            cmdsOut.emergencySmsRequest = doc["emergency_sms_request"] | false;
            cmdsOut.resetAlarm = doc["reset_alarm"] | false;
            cmdsOut.geofenceRadius = doc["geofence_radius"] | 20.0f;
            cmdsOut.geofenceEnabled = doc["geofence_enabled"] | true;
            cmdsOut.autoCutoffGeofence = doc["auto_cutoff_geofence"] | false;
            cmdsOut.anchorLat = doc["anchor_lat"] | 0.0;
            cmdsOut.anchorLng = doc["anchor_lng"] | 0.0;
            cmdsOut.lastCommandTime = doc["last_command_time"] | 0;
            cmdsOut.lastCommandKey = doc["last_command_key"] | "";
            return true;
        }
    }
    return false;
}

bool FirebaseSyncClient::pushLogEvent(const String &eventType, const String &message, double lat, double lng, float speed) {
    // Validasi Koordinat: Jangan pernah mencatat latitude/longitude = 0
    if (lat == 0.0 || isnan(lat)) lat = -5.460095;
    if (lng == 0.0 || isnan(lng)) lng = 122.616677;

    StaticJsonDocument<384> doc;
    doc["timestamp"] = millis() / 1000;
    doc["event_type"] = eventType;
    doc["message"] = message;
    doc["latitude"] = lat;
    doc["longitude"] = lng;
    doc["speed"] = speed;

    char logBuffer[384];
    serializeJson(doc, logBuffer, sizeof(logBuffer));

    String path = "/vehicles/" + _deviceId + "/logs.json";
    String response;
    return _sendRestRequest("POST", path, logBuffer, response);
}

bool FirebaseSyncClient::acknowledgeCommand(const String &commandKey) {
    StaticJsonDocument<64> doc;
    doc[commandKey] = false;
    char ackBuffer[64];
    serializeJson(doc, ackBuffer, sizeof(ackBuffer));

    String path = "/vehicles/" + _deviceId + "/controls.json";
    String response;
    return _sendRestRequest("PATCH", path, ackBuffer, response);
}

bool FirebaseSyncClient::syncEngineLockState(bool locked) {
    // Multi-location PATCH menjaga command persisten dan status perangkat tetap
    // identik tanpa mengubah last_command_time milik dashboard.
    StaticJsonDocument<128> doc;
    doc["controls/lock_engine"] = locked;
    doc["status/engine_locked"] = locked;

    char payloadBuffer[128];
    serializeJson(doc, payloadBuffer, sizeof(payloadBuffer));

    String path = "/vehicles/" + _deviceId + ".json";
    String response;
    return _sendRestRequest("PATCH", path, payloadBuffer, response);
}
