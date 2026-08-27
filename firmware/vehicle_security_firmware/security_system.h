#ifndef SECURITY_SYSTEM_H
#define SECURITY_SYSTEM_H

#include <Arduino.h>
#include <EEPROM.h>
#include "config.h"
#include "actuators.h"
#include "gps_manager.h"
#include "gsm_sim800l.h"

// EEPROM Address Map untuk menyimpan pengaturan geofence
#define EEPROM_SIZE             64
#define EEPROM_MAGIC_BYTE       0xA6
#define EEPROM_ADDR_MAGIC       0
#define EEPROM_ADDR_ENABLED     1
#define EEPROM_ADDR_AUTOCUTOFF  2
#define EEPROM_ADDR_RADIUS      4    // float (4 bytes)
#define EEPROM_ADDR_ANCHOR_LAT  8    // double (8 bytes)
#define EEPROM_ADDR_ANCHOR_LNG  16   // double (8 bytes)

enum SecurityState {
    SEC_DISARMED,
    SEC_ARMED,
    SEC_ALARM_TRIGGERED
};

class SecuritySystem {
public:
    SecuritySystem(uint8_t sw420Pin = PIN_SW420);
    
    void begin(ActuatorManager* actuators, GPSManager* gps, GSMSim800L* gsm);
    void update();
    
    void arm();
    void disarm();
    void triggerAlarm(const String &reason = "VIBRATION");
    void resetAlarm();
    
    bool isArmed() const;
    bool isAlarmActive() const;
    SecurityState getState() const;
    String getStateString() const;
    String getLastAlarmReason() const;
    
    bool isVibrationDetected() const;
    uint32_t getVibrationCount() const;

    // Geofence Radius 20m Methods
    void checkGeofence(double currentLat, double currentLng, bool gpsFixed);
    double getAnchorLatitude() const { return _anchorLat; }
    double getAnchorLongitude() const { return _anchorLng; }
    bool isGeofenceActive() const { return _geofenceArmed; }
    float getGeofenceRadius() const { return _geofenceRadius; }
    void setGeofenceRadius(float radiusMeters) { _geofenceRadius = radiusMeters; }
    void setGeofenceEnabled(bool enabled);
    void setAutoCutoffGeofence(bool enabled) { _autoCutoffGeofence = enabled; }
    bool isAutoCutoffGeofence() const { return _autoCutoffGeofence; }
    void setAnchorPoint(double lat, double lng) { _anchorLat = lat; _anchorLng = lng; }
    double calculateDistanceMeters(double lat1, double lon1, double lat2, double lon2);

    // EEPROM Persistence — menyimpan/memuat pengaturan geofence tanpa internet
    void saveGeofenceToEEPROM();
    void loadGeofenceFromEEPROM();

    // Auto-arm geofence saat GPS fix pertama kali diperoleh (tanpa internet)
    void autoArmGeofenceOnFix(double lat, double lng);

private:
    uint8_t _sw420Pin;
    SecurityState _state;
    ActuatorManager* _actuators;
    GPSManager* _gps;
    GSMSim800L* _gsm;
    
    bool _vibrationFlag;
    bool _lastSensorState;
    uint8_t _shockCountInWindow;
    uint32_t _windowStartTime;
    uint32_t _lastPreWarnTime;
    uint32_t _lastVibrationTime;
    uint32_t _vibrationCounter;
    uint32_t _alarmTriggeredTime;
    String _lastAlarmReason;
    bool _smsAlertSent;

    // Geofence Tracking
    double _anchorLat;
    double _anchorLng;
    bool _geofenceArmed;
    float _geofenceRadius;
    bool _geofenceAlertSent;
    bool _autoCutoffGeofence;
    bool _geofenceAutoArmed; // flag: sudah auto-arm pada GPS fix pertama
    bool _geofenceConfigLoaded;
    bool _pendingSmsRetry;
    uint8_t _smsRetryCount;
    uint32_t _lastSmsRetryTime;
    String _pendingSmsText;
    String _pendingSmsType;

    void _handleVibrationSensor();
    void _sendEmergencySMS();
    bool _sendGeofenceSMS(double distMeters, double lat, double lng, float speed);
    bool _sendOrQueueSms(const String &text, const String &type);
};

#endif // SECURITY_SYSTEM_H
