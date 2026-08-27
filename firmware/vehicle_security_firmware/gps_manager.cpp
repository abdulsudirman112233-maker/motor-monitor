#include "gps_manager.h"

GPSManager::GPSManager(uint8_t rxPin, uint8_t txPin) 
    : _gpsSerial(rxPin, txPin), _lastPrintTime(0), _movementConfirmations(0),
      _candidateLatitude(0.0), _candidateLongitude(0.0),
      _lastAcceptedPositionTime(0) {
    _currentData.latitude = -5.460095;  // Koordinat Real Baubau
    _currentData.longitude = 122.616677;
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

void GPSManager::listen() {
    _gpsSerial.listen();
}

void GPSManager::update() {
    _gpsSerial.listen();
    while (_gpsSerial.available() > 0) {
        char c = _gpsSerial.read();
        _tinyGps.encode(c);
    }

    // isValid() tetap true untuk data lama. Posisi hanya boleh diproses ketika
    // kalimat NMEA baru selesai diterima dan kualitas fix mencukupi.
    if (_tinyGps.location.isUpdated() && _tinyGps.location.isValid()) {
        const double newLat = _tinyGps.location.lat();
        const double newLng = _tinyGps.location.lng();
        const uint32_t sats = (_tinyGps.satellites.isValid() && _tinyGps.satellites.age() <= 5000) ?
            _tinyGps.satellites.value() : 0;
        const double hdop = (_tinyGps.hdop.isValid() && _tinyGps.hdop.age() <= 5000) ?
            _tinyGps.hdop.hdop() : 99.9;
        const float rawSpeed = (_tinyGps.speed.isValid() && _tinyGps.speed.age() <= 2000) ?
            _tinyGps.speed.kmph() : 0.0f;
        const bool qualityOk = sats >= 4 && hdop <= 5.0 && _tinyGps.location.age() <= 2000;

        if (qualityOk) {
            const bool firstFix = !_currentData.isValid || _currentData.lastFixTime == 0;
            const double displacement = firstFix ? 0.0 : TinyGPSPlus::distanceBetween(
                _currentData.latitude, _currentData.longitude, newLat, newLng);
            const uint32_t now = millis();
            const float secondsSinceAccepted = _lastAcceptedPositionTime == 0 ? 1.0f :
                max(0.2f, (now - _lastAcceptedPositionTime) / 1000.0f);
            const float derivedSpeed = firstFix ? 0.0f :
                (float)(displacement / secondsSinceAccepted * 3.6);
            const float movementSpeed = rawSpeed >= 2.5f ? rawSpeed : derivedSpeed;

            bool acceptPosition = firstFix;
            if (!firstFix) {
                // Saat diam, tahan drift sampai dua fix berurutan menunjukkan
                // perpindahan nyata. Deadband 5 m menahan drift tanpa membuat
                // kendaraan yang benar-benar berjalan terasa terlambat.
                if (_currentData.speedKmh <= 2.5f && displacement < 5.0) {
                    _movementConfirmations = 0;
                    acceptPosition = false;
                } else if (_currentData.speedKmh <= 2.5f) {
                    const double candidateStep = (_movementConfirmations == 0) ? 0.0 :
                        TinyGPSPlus::distanceBetween(_candidateLatitude, _candidateLongitude, newLat, newLng);
                    _candidateLatitude = newLat;
                    _candidateLongitude = newLng;
                    if (movementSpeed >= 3.0f) {
                        if (_movementConfirmations == 0) {
                            _movementConfirmations = 1;
                        } else if (candidateStep >= 0.5 && candidateStep <= 40.0) {
                            _movementConfirmations++;
                        } else {
                            _movementConfirmations = 1;
                        }
                    } else {
                        _movementConfirmations = 0;
                    }
                    acceptPosition = _movementConfirmations >= 2;
                } else {
                    acceptPosition = true;
                }
            }

            if (acceptPosition) {
                _currentData.latitude = newLat;
                _currentData.longitude = newLng;
                _movementConfirmations = 0;
                _lastAcceptedPositionTime = now;
            }

            _currentData.isValid = true;
            _currentData.lastFixTime = millis();
            _currentData.speedKmh = !firstFix && acceptPosition && movementSpeed >= 2.5f ? movementSpeed : 0.0f;

            char urlBuffer[64];
            snprintf(urlBuffer, sizeof(urlBuffer), "https://maps.google.com/?q=%.6f,%.6f",
                     _currentData.latitude, _currentData.longitude);
            _currentData.googleMapsUrl = String(urlBuffer);
        }
    }

    // Fix kedaluwarsa tidak boleh terus dianggap hidup hanya karena pernah valid.
    if (_currentData.lastFixTime == 0 || millis() - _currentData.lastFixTime > 10000) {
        _currentData.isValid = false;
        _currentData.speedKmh = 0.0f;
        _movementConfirmations = 0;
    }

    if (_tinyGps.altitude.isValid()) {
        _currentData.altitude = _tinyGps.altitude.meters();
    }
    
    if (_tinyGps.speed.isUpdated() && _tinyGps.speed.isValid() && _currentData.speedKmh > 0.0f) {
        float spd = _tinyGps.speed.kmph();
        // Filter Deadband Satelit GPS: Kecepatan < 2.5 km/h adalah noise/drift satelit saat motor diam
        if (spd < 2.5f) spd = 0.0f;
        _currentData.speedKmh = spd;
    }

    if (_tinyGps.course.isUpdated() && _tinyGps.course.isValid() && _currentData.speedKmh > 0.0f) {
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
