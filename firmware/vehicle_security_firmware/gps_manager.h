#ifndef GPS_MANAGER_H
#define GPS_MANAGER_H

#include <Arduino.h>
#include <SoftwareSerial.h>
#include <TinyGPS++.h>
#include "config.h"

struct GPSData {
    double latitude;
    double longitude;
    double altitude;
    float speedKmh;
    float heading;
    uint32_t satellites;
    double hdop;
    bool isValid;
    uint32_t lastFixTime;
    String dateTimeString;
    String googleMapsUrl;
};

class GPSManager {
public:
    GPSManager(uint8_t rxPin, uint8_t txPin);
    void begin(uint32_t baudRate = GPS_BAUD_RATE);
    void update();
    
    bool hasValidFix() const;
    GPSData getData() const;
    
    double getLatitude() const;
    double getLongitude() const;
    float getSpeed() const;
    float getHeading() const;
    uint32_t getSatellites() const;
    String getGoogleMapsLink() const;
    
    void printDebugInfo() const;

private:
    SoftwareSerial _gpsSerial;
    TinyGPSPlus _tinyGps;
    GPSData _currentData;
    uint32_t _lastPrintTime;
};

#endif // GPS_MANAGER_H
