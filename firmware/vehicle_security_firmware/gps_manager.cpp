#include "gps_manager.h"

GPSManager::GPSManager(uint8_t rxPin, uint8_t txPin) 
    : _gpsSerial(rxPin, txPin), _lastPrintTime(0) {
    _currentData.latitude = 0.0;
    _currentData.longitude = 0.0;
    _currentData.altitude = 0.0;
    _currentData.speedKmh = 0.0;
    _currentData.heading = 0.0;
    _currentData.satellites = 0;
    _currentData.hdop = 99.9;
    _currentData.isValid = false;
    _currentData.lastFixTime = 0;
    _currentData.dateTimeString = "N/A";
    _currentData.googleMapsUrl = "N/A";
}

void GPSManager::begin(uint32_t baudRate) {
    _gpsSerial.begin(baudRate);
    Serial.println(F("[GPS] Inisialisasi SoftwareSerial GPS Neo-6M pada Baud 9600..."));
}

void GPSManager::update() {
    _gpsSerial.listen();
    while (_gpsSerial.available() > 0) {
        char c = _gpsSerial.read();
        _tinyGps.encode(c);
    }

    if (_tinyGps.location.isValid()) {
        _currentData.latitude = _tinyGps.location.lat();
        _currentData.longitude = _tinyGps.location.lng();
        _currentData.isValid = true;
        _currentData.lastFixTime = millis();
        
        char urlBuffer[64];
        snprintf(urlBuffer, sizeof(urlBuffer), "https://maps.google.com/?q=%.6f,%.6f", 
                 _currentData.latitude, _currentData.longitude);
        _currentData.googleMapsUrl = String(urlBuffer);
    } else {
        // Jika tidak ada update dalam 15 detik, anggap fix hilang
        if (millis() - _currentData.lastFixTime > 15000) {
            _currentData.isValid = false;
        }
    }

    if (_tinyGps.altitude.isValid()) {
        _currentData.altitude = _tinyGps.altitude.meters();
    }
    
    if (_tinyGps.speed.isValid()) {
        float spd = _tinyGps.speed.kmph();
        // Filter Deadband Satelit GPS: Kecepatan < 2.5 km/h adalah noise/drift satelit saat motor diam
        if (spd < 2.5f) spd = 0.0f;
        _currentData.speedKmh = spd;
    } else {
        _currentData.speedKmh = 0.0;
    }

    if (_tinyGps.course.isValid()) {
        _currentData.heading = _tinyGps.course.deg();
    }

    if (_tinyGps.satellites.isValid()) {
        _currentData.satellites = _tinyGps.satellites.value();
    }

    if (_tinyGps.hdop.isValid()) {
        _currentData.hdop = _tinyGps.hdop.hdop();
    }

    if (_tinyGps.date.isValid() && _tinyGps.time.isValid()) {
        char dtBuf[32];
        snprintf(dtBuf, sizeof(dtBuf), "%04d-%02d-%02d %02d:%02d:%02d UTC",
                 _tinyGps.date.year(), _tinyGps.date.month(), _tinyGps.date.day(),
                 _tinyGps.time.hour(), _tinyGps.time.minute(), _tinyGps.time.second());
        _currentData.dateTimeString = String(dtBuf);
    }
}

bool GPSManager::hasValidFix() const {
    return _currentData.isValid;
}

GPSData GPSManager::getData() const {
    return _currentData;
}

double GPSManager::getLatitude() const {
    return _currentData.latitude;
}

double GPSManager::getLongitude() const {
    return _currentData.longitude;
}

float GPSManager::getSpeed() const {
    return _currentData.speedKmh;
}

float GPSManager::getHeading() const {
    return _currentData.heading;
}

uint32_t GPSManager::getSatellites() const {
    return _currentData.satellites;
}

String GPSManager::getGoogleMapsLink() const {
    return _currentData.googleMapsUrl;
}

void GPSManager::printDebugInfo() const {
    Serial.print(F("[GPS] Lat: "));
    Serial.print(_currentData.latitude, 6);
    Serial.print(F(" | Lng: "));
    Serial.print(_currentData.longitude, 6);
    Serial.print(F(" | Speed: "));
    Serial.print(_currentData.speedKmh, 1);
    Serial.print(F(" km/h | Sats: "));
    Serial.print(_currentData.satellites);
    Serial.print(F(" | Valid: "));
    Serial.println(_currentData.isValid ? F("YES") : F("NO FIX"));
}
