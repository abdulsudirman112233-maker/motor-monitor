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

    Serial.print(F("[WIFI] Menghubungkan ke SSID: "));
    Serial.println(WIFI_SSID);
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

void FirebaseSyncClient::updateWiFi() {
    if (WiFi.status() != WL_CONNECTED) {
        if (millis() - _lastWiFiRetry > 10000) {
            _lastWiFiRetry = millis();
            Serial.println(F("[WIFI] Mencoba menyambung kembali ke WiFi..."));
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
    // Hanya tambahkan ?auth= jika bukan Web API Key (AIzaSy) karena RTDB REST memerlukan Database Secret / JWT jika database dikunci
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
    // 1. Coba via WiFi jika terhubung
    if (WiFi.status() == WL_CONNECTED) {
        WiFiClientSecure client;
        client.setInsecure(); // Bypass verifikasi sertifikat SSL untuk menghemat RAM
        client.setBufferSizes(512, 512);
        client.setTimeout(4500);

        HTTPClient http;
        String fullUrl = _buildUrl(path);

        if (http.begin(client, fullUrl)) {
            http.addHeader("Content-Type", "application/json");
            http.addHeader("Connection", "close");
            http.setTimeout(4500);
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

    // 2. Fallback otomatis ke SIM800L GPRS jika WiFi tidak terhubung
    if (_gsm) {
        return _sendGprsRestRequest(method, path, payload, responseOut);
    }

    return false;
}

bool FirebaseSyncClient::pushTelemetry(const GPSData &gps, const GSMStatus &gsm, const SecuritySystem &sec, const ActuatorManager &act, float batteryVoltage) {
    // Bangun JSON Payload Telemetri (menggunakan static buffer tanpa fragmentasi memori)
    StaticJsonDocument<512> doc;
    doc["latitude"] = gps.latitude;
    doc["longitude"] = gps.longitude;
    doc["altitude"] = gps.altitude;
    doc["speed"] = gps.speedKmh;
    doc["heading"] = gps.heading;
    doc["satellites"] = gps.satellites;
    doc["hdop"] = gps.hdop;
    doc["gps_fixed"] = gps.isValid;
    doc["gsm_csq"] = gsm.csq;
    doc["gsm_signal_percent"] = gsm.signalPercent;
    doc["gsm_network"] = gsm.operatorName;
    doc["battery_voltage"] = batteryVoltage;
    doc["power_source"] = (batteryVoltage > 11.0) ? "ACCU_12V" : "LOW_BATTERY";
    doc["vibration_detected"] = sec.isVibrationDetected();
    doc["engine_running"] = (!act.isEngineLocked() && gps.speedKmh > 3.0);
    doc["connection_mode"] = isWiFiConnected() ? "WIFI_ONLINE" : "GPRS_FALLBACK";
    doc["timestamp"] = millis() / 1000;

    char payloadBuffer[512];
    serializeJson(doc, payloadBuffer, sizeof(payloadBuffer));

    String path = "/vehicles/" + _deviceId + "/telemetry.json";
    String response;
    bool ok = _sendRestRequest("PATCH", path, payloadBuffer, response);

    // Update Status Keamanan
    StaticJsonDocument<256> statusDoc;
    statusDoc["armed"] = sec.isArmed();
    statusDoc["alarm_active"] = sec.isAlarmActive();
    statusDoc["engine_locked"] = act.isEngineLocked();
    statusDoc["theft_alert"] = sec.isAlarmActive();
    statusDoc["last_alarm_reason"] = sec.getLastAlarmReason();

    char statusBuffer[256];
    serializeJson(statusDoc, statusBuffer, sizeof(statusBuffer));
    String statusPath = "/vehicles/" + _deviceId + "/status.json";
    _sendRestRequest("PATCH", statusPath, statusBuffer, response);

    return ok;
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
            cmdsOut.lastCommandTime = doc["last_command_time"] | 0;
            return true;
        }
    }
    return false;
}

bool FirebaseSyncClient::pushLogEvent(const String &eventType, const String &message, double lat, double lng, float speed) {
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
