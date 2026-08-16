#ifndef SECURITY_SYSTEM_H
#define SECURITY_SYSTEM_H

#include <Arduino.h>
#include "config.h"
#include "actuators.h"
#include "gps_manager.h"
#include "gsm_sim800l.h"

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

    // Geofence Radius 75m Methods
    void checkGeofence(double currentLat, double currentLng, bool gpsFixed);
    double getAnchorLatitude() const { return _anchorLat; }
    double getAnchorLongitude() const { return _anchorLng; }
    bool isGeofenceActive() const { return _geofenceArmed; }
    float getGeofenceRadius() const { return _geofenceRadius; }
    void setGeofenceRadius(float radiusMeters) { _geofenceRadius = radiusMeters; }
    double calculateDistanceMeters(double lat1, double lon1, double lat2, double lon2);

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

    void _handleVibrationSensor();
    void _sendEmergencySMS();
    void _sendGeofenceSMS(double distMeters, double lat, double lng, float speed);
};

#endif // SECURITY_SYSTEM_H
