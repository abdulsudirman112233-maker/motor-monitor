#ifndef FIREBASE_CLIENT_H
#define FIREBASE_CLIENT_H

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include "config.h"
#include "gps_manager.h"
#include "gsm_sim800l.h"
#include "security_system.h"
#include "actuators.h"

struct ControlCommands {
    bool lockEngine;
    bool armed;
    bool triggerPanic;
    bool findVehicle;
    bool emergencySmsRequest;
    bool resetAlarm;
    float geofenceRadius;
    bool geofenceEnabled;
    double anchorLat;
    double anchorLng;
    uint32_t lastCommandTime;
};

class FirebaseSyncClient {
public:
    FirebaseSyncClient();
    
    void begin(GSMSim800L* gsm = nullptr);
    void updateWiFi();
    
    bool isWiFiConnected() const;
    bool isGprsActive() const;
    
    bool pushTelemetry(const GPSData &gps, const GSMStatus &gsm, const SecuritySystem &sec, const ActuatorManager &act, float batteryVoltage);
    bool fetchControlCommands(ControlCommands &cmdsOut);
    bool pushLogEvent(const String &eventType, const String &message, double lat, double lng, float speed);
    bool acknowledgeCommand(const String &commandKey);

private:
    GSMSim800L* _gsm;
    String _host;
    String _auth;
    String _deviceId;
    uint32_t _lastWiFiRetry;
    uint32_t _lastGprsRetry;
    
    String _buildUrl(const String &path);
    bool _sendRestRequest(const String &method, const String &path, const char* payload, String &responseOut);
    bool _sendGprsRestRequest(const String &method, const String &path, const char* payload, String &responseOut);
};

#endif // FIREBASE_CLIENT_H
