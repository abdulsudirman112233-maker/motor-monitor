#include "security_system.h"

SecuritySystem::SecuritySystem(uint8_t sw420Pin)
    : _sw420Pin(sw420Pin), _state(SEC_DISARMED), _actuators(nullptr),
      _gps(nullptr), _gsm(nullptr), _vibrationFlag(false),
      _lastSensorState(LOW), _shockCountInWindow(0), _windowStartTime(0),
      _lastPreWarnTime(0), _lastVibrationTime(0), _vibrationCounter(0),
      _alarmTriggeredTime(0), _lastAlarmReason("NONE"), _smsAlertSent(false),
      _anchorLat(0.0), _anchorLng(0.0), _geofenceArmed(false),
      _geofenceRadius(GEOFENCE_DEFAULT_RADIUS), _geofenceAlertSent(false),
      _autoCutoffGeofence(false), _geofenceAutoArmed(false), _geofenceConfigLoaded(false),
      _pendingSmsRetry(false), _smsRetryCount(0), _lastSmsRetryTime(0),
      _pendingSmsText(""), _pendingSmsType("NONE") {
}

void SecuritySystem::begin(ActuatorManager* actuators, GPSManager* gps, GSMSim800L* gsm) {
    _actuators = actuators;
    _gps = gps;
    _gsm = gsm;

    pinMode(_sw420Pin, INPUT);
    _lastSensorState = digitalRead(_sw420Pin);
    _state = SEC_ARMED; // Default saat dinyalakan: Sistem dalam keadaan siaga/ARMED

    // Muat pengaturan geofence dari EEPROM (tersimpan dari sesi sebelumnya)
    loadGeofenceFromEEPROM();
    
    if (!_geofenceConfigLoaded && _gps && _gps->hasValidFix()) {
        _anchorLat = _gps->getLatitude();
        _anchorLng = _gps->getLongitude();
        _geofenceArmed = true;
    }
    
    Serial.println(F("[SECURITY] Sistem Keamanan Aktif. Mode: ARMED (Sensitivitas Cerdas & Geofence Aktif)."));
}

void SecuritySystem::update() {
    _handleVibrationSensor();

    // Retry SMS keamanan tanpa delay(). Percobaan pertama tetap langsung,
    // berikutnya maksimal dua kali dengan jeda 30 detik.
    if (_pendingSmsRetry && millis() - _lastSmsRetryTime >= 30000) {
        _lastSmsRetryTime = millis();
        _smsRetryCount++;
        if (_gsm && _gsm->sendSMS(OWNER_PHONE_NUMBER, _pendingSmsText, _pendingSmsType)) {
            _pendingSmsRetry = false;
            Serial.println(F("[SMS RETRY] Pesan keamanan berhasil dikirim."));
        } else if (_smsRetryCount >= 3) {
            _pendingSmsRetry = false;
            Serial.println(F("[SMS RETRY] Dihentikan setelah 3 percobaan gagal."));
        }
    }

    // Jika sedang dalam kondisi alarm terpicu
    if (_state == SEC_ALARM_TRIGGERED) {
        // Cek timeout sirene alarm agar tidak menguras baterai terus-menerus
        if (millis() - _alarmTriggeredTime > VIBRATION_ALARM_HOLD_MS) {
            Serial.println(F("[SECURITY] Alarm Sirene Timeout (20s). Kembali ke status siaga ARMED."));
            if (_actuators) _actuators->stopBuzzer();
            _state = SEC_ARMED;
            _smsAlertSent = false;
        }
    }
}

void SecuritySystem::_handleVibrationSensor() {
    int sensorVal = digitalRead(_sw420Pin);
    uint32_t now = millis();

    // Reset akumulasi getaran jika jendela waktu VIBRATION_WINDOW_MS terlewati
    if (_shockCountInWindow > 0 && (now - _windowStartTime > VIBRATION_WINDOW_MS)) {
        _shockCountInWindow = 0;
    }

    // Deteksi transisi pulsa getaran fisik dari sensor SW-420
    if (sensorVal == HIGH && _lastSensorState == LOW) {
        // Filter debouncing antar pulsa
        if (now - _lastVibrationTime > VIBRATION_DEBOUNCE_MS) {
            _lastVibrationTime = now;
            _vibrationCounter++;
            _vibrationFlag = true;

            if (_shockCountInWindow == 0) {
                _windowStartTime = now;
            }
            _shockCountInWindow++;

            Serial.print(F("[SECURITY] Pulsa getaran terdeteksi: "));
            Serial.print(_shockCountInWindow);
            Serial.print(F("/"));
            Serial.print(VIBRATION_SHOCK_THRESHOLD);
            Serial.print(F(" (Total: #"));
            Serial.print(_vibrationCounter);
            Serial.println(F(")"));

            // Hanya proses eskalasi jika sistem dalam keadaan ARMED (Terkunci)
            if (_state == SEC_ARMED) {
                // TINGKAT 1: Getaran Ringan (1-3 pulsa) -> Hanya bunyikan beep peringatan lokal pendek (Tanpa Alarm Penuh & Tanpa SMS)
                if (_shockCountInWindow < VIBRATION_SHOCK_THRESHOLD) {
                    if (now - _lastPreWarnTime > 2500) {
                        _lastPreWarnTime = now;
                        Serial.println(F("[SECURITY] Getaran ringan terdeteksi -> Beep peringatan lokal (Bukan pencurian)."));
                        if (_actuators) _actuators->triggerFinderChirp();
                    }
                }
                // TINGKAT 2: Getaran Keras / Beruntun (>= 4 pulsa dalam 3 detik) -> Pencurian terkonfirmasi!
                else {
                    Serial.println(F("[SECURITY] !!! GETARAN PENCURIAN TERKONFIRMASI !!! Memicu Sirene & SMS Darurat."));
                    _shockCountInWindow = 0;
                    triggerAlarm("VIBRATION_THEFT_DETECTED");
                }
            }
        }
    } else if (sensorVal == LOW) {
        if (now - _lastVibrationTime > 3000) {
            _vibrationFlag = false;
        }
    }

    _lastSensorState = sensorVal;
}

void SecuritySystem::arm() {
    _state = SEC_ARMED;
    _lastAlarmReason = "NONE";
    _smsAlertSent = false;
    _geofenceAlertSent = false;
    
    // ARM/DISARM keamanan tidak boleh mengubah pilihan geofence.
    Serial.print(F("[SECURITY] Mode Keamanan: ARMED | Geofence tetap "));
    Serial.println(_geofenceArmed ? F("AKTIF") : F("NONAKTIF"));
    if (_actuators) {
        _actuators->triggerArmChirp();
    }
}

void SecuritySystem::disarm() {
    _state = SEC_DISARMED;
    _lastAlarmReason = "NONE";
    _smsAlertSent = false;
    _geofenceAlertSent = false;
    
    Serial.print(F("[SECURITY] Mode Keamanan: DISARMED | Geofence tetap "));
    Serial.println(_geofenceArmed ? F("AKTIF") : F("NONAKTIF"));
    if (_actuators) {
        _actuators->stopBuzzer();
        _actuators->triggerDisarmChirp();
    }
}

void SecuritySystem::setGeofenceEnabled(bool enabled) {
    _geofenceArmed = enabled;
    _geofenceAutoArmed = true; // pilihan manual/cloud tidak boleh ditimpa auto-arm

    if (enabled) {
        _geofenceAlertSent = false;
        return;
    }

    // OFF berarti seluruh state breach geofence juga harus berhenti.
    _geofenceAlertSent = false;
    if (_pendingSmsType == "GEOFENCE") {
        _pendingSmsRetry = false;
        _pendingSmsText = "";
        _pendingSmsType = "NONE";
        _smsRetryCount = 0;
    }

    if (_lastAlarmReason == "GEOFENCE_BREACH") {
        if (_actuators) _actuators->stopBuzzer();
        _state = SEC_ARMED;
        _lastAlarmReason = "NONE";
        _smsAlertSent = false;
        _alarmTriggeredTime = 0;
        Serial.println(F("[GEOFENCE] Dinonaktifkan: alarm breach dan sirene dibersihkan."));
    }
}

double SecuritySystem::calculateDistanceMeters(double lat1, double lon1, double lat2, double lon2) {
    if (lat1 == 0.0 || lon1 == 0.0 || lat2 == 0.0 || lon2 == 0.0) return 0.0;
    
    double dLat = (lat2 - lat1) * (PI / 180.0);
    double dLon = (lon2 - lon1) * (PI / 180.0);
    
    double a = sin(dLat / 2.0) * sin(dLat / 2.0) +
               cos(lat1 * (PI / 180.0)) * cos(lat2 * (PI / 180.0)) *
               sin(dLon / 2.0) * sin(dLon / 2.0);
               
    double c = 2.0 * atan2(sqrt(a), sqrt(1.0 - a));
    return 6371000.0 * c; // Radius Bumi 6,371 km -> Hasil dalam meter
}

void SecuritySystem::checkGeofence(double currentLat, double currentLng, bool gpsFixed) {
    // Geofence berfungsi secara INDEPENDEN dari status ARMED/DISARMED.
    // Cukup syarat: _geofenceArmed == true (toggle dari web/SMS) dan GPS valid.
    if (!_geofenceArmed || !gpsFixed) return;
    
    // Inisialisasi titik pusat jika belum tersetel
    if (_anchorLat == 0.0 && _anchorLng == 0.0) {
        _anchorLat = currentLat;
        _anchorLng = currentLng;
        Serial.print(F("[GEOFENCE] Titik anchor otomatis ditetapkan: "));
        Serial.print(_anchorLat, 6);
        Serial.print(F(", "));
        Serial.println(_anchorLng, 6);
        return;
    }

    double distance = calculateDistanceMeters(_anchorLat, _anchorLng, currentLat, currentLng);
    
    // Jika motor berpindah melebihi radius batas aman
    if (distance > _geofenceRadius) {
        if (!_geofenceAlertSent) {
            _geofenceAlertSent = true;
            Serial.print(F("[GEOFENCE BREACH] Motor berpindah sejauh: "));
            Serial.print(distance, 1);
            Serial.print(F("m (Batas Aman: "));
            Serial.print(_geofenceRadius, 0);
            Serial.println(F("m) -> Geofence Terlangar!"));

            // 1. Matikan Mesin Otomatis (Relay Cut-off) HANYA jika auto-cutoff diaktifkan
            if (_autoCutoffGeofence && _actuators) {
                Serial.println(F("[GEOFENCE] Auto Cut-Off AKTIF -> Mematikan mesin & membunyikan sirene!"));
                _actuators->setEngineLocked(true);
                _actuators->triggerPanicSiren();
            } else {
                Serial.println(F("[GEOFENCE] Auto Cut-Off NONAKTIF -> Hanya mengirim SMS notifikasi."));
                if (_actuators) _actuators->triggerPanicSiren();
            }

            // 2. Kirim SMS Notifikasi ke Pemilik
            float spd = (_gps && _gps->hasValidFix()) ? _gps->getSpeed() : 0.0;
            _sendGeofenceSMS(distance, currentLat, currentLng, spd);
            // Satu breach menghasilkan satu jenis SMS. Jika percobaan pertama
            // gagal, antrean millis() akan mencoba ulang tanpa membuat SMS ganda.
            _smsAlertSent = true;

            // 3. Picu status Alarm Sistem
            triggerAlarm("GEOFENCE_BREACH");
        }
    } else {
        // Reset flag alert jika motor kembali ke dalam radius aman
        if (distance <= (_geofenceRadius * 0.6)) {
            _geofenceAlertSent = false;
        }
    }
}

bool SecuritySystem::_sendGeofenceSMS(double distMeters, double lat, double lng, float speed) {
    if (!_gsm) return false;

    String mapsUrl = "https://maps.google.com/?q=" + String(lat, 6) + "," + String(lng, 6);
    // Dijaga di bawah 160 karakter agar SIM800L mengirim sebagai satu SMS.
    String smsText = "ALARM GEOFENCE!\n";
    smsText += "Jarak " + String(distMeters, 0) + "m > " + String(_geofenceRadius, 0) + "m";
    smsText += " | Spd " + String(speed, 0) + "km/h\n";
    smsText += mapsUrl + "\n";
    smsText += (_autoCutoffGeofence ? "Mesin: CUT-OFF" : "Mesin: normal");

    return _sendOrQueueSms(smsText, "GEOFENCE");
}

bool SecuritySystem::_sendOrQueueSms(const String &text, const String &type) {
    if (!_gsm) return false;
    const bool sent = _gsm->sendSMS(OWNER_PHONE_NUMBER, text, type);
    if (!sent) {
        _pendingSmsText = text;
        _pendingSmsType = type;
        _pendingSmsRetry = true;
        _smsRetryCount = 1;
        _lastSmsRetryTime = millis();
    } else {
        _pendingSmsRetry = false;
    }
    return sent;
}

void SecuritySystem::triggerAlarm(const String &reason) {
    if (_state == SEC_ALARM_TRIGGERED && _smsAlertSent) return;

    _state = SEC_ALARM_TRIGGERED;
    _lastAlarmReason = reason;
    _alarmTriggeredTime = millis();
    
    Serial.print(F("[SECURITY] !!! ALARM PENCURIAN TERPICU !!! Alasan: "));
    Serial.println(reason);

    // 1. Bunyikan sirene buzzer
    if (_actuators) {
        _actuators->triggerPanicSiren();
    }

    // 2. Kirim SMS Darurat ke Pemilik jika belum terkirim
    if (!_smsAlertSent) {
        _sendEmergencySMS();
        _smsAlertSent = true;
    }
}

void SecuritySystem::resetAlarm() {
    if (_state == SEC_ALARM_TRIGGERED) {
        Serial.println(F("[SECURITY] Reset Alarm manual."));
        if (_actuators) _actuators->stopBuzzer();
        _state = SEC_ARMED;
        _smsAlertSent = false;
        _geofenceAlertSent = false;
    }
}

void SecuritySystem::_sendEmergencySMS() {
    if (!_gsm) return;

    String mapsUrl = "Tidak ada sinyal GPS";
    double lat = 0.0, lng = 0.0;
    float spd = 0.0;

    if (_gps && _gps->hasValidFix()) {
        lat = _gps->getLatitude();
        lng = _gps->getLongitude();
        spd = _gps->getSpeed();
        mapsUrl = _gps->getGoogleMapsLink();
    }

    String smsText = "ALARM MOTOR! Getaran terdeteksi.\n";
    smsText += "Spd " + String(spd, 0) + "km/h\n";
    smsText += mapsUrl + "\n";
    smsText += "Balas #MATIKAN untuk cut-off.";

    _sendOrQueueSms(smsText, "ALARM");
}

bool SecuritySystem::isArmed() const {
    return _state == SEC_ARMED || _state == SEC_ALARM_TRIGGERED;
}

bool SecuritySystem::isAlarmActive() const {
    return _state == SEC_ALARM_TRIGGERED;
}

SecurityState SecuritySystem::getState() const {
    return _state;
}

String SecuritySystem::getStateString() const {
    switch (_state) {
        case SEC_DISARMED: return "DISARMED";
        case SEC_ARMED: return "ARMED";
        case SEC_ALARM_TRIGGERED: return "ALARM_TRIGGERED";
        default: return "UNKNOWN";
    }
}

String SecuritySystem::getLastAlarmReason() const {
    return _lastAlarmReason;
}

bool SecuritySystem::isVibrationDetected() const {
    return _vibrationFlag;
}

uint32_t SecuritySystem::getVibrationCount() const {
    return _vibrationCounter;
}

void SecuritySystem::saveGeofenceToEEPROM() {
    EEPROM.begin(EEPROM_SIZE);
    EEPROM.write(EEPROM_ADDR_MAGIC, EEPROM_MAGIC_BYTE);
    EEPROM.write(EEPROM_ADDR_ENABLED, _geofenceArmed ? 1 : 0);
    EEPROM.write(EEPROM_ADDR_AUTOCUTOFF, _autoCutoffGeofence ? 1 : 0);
    EEPROM.put(EEPROM_ADDR_RADIUS, _geofenceRadius);
    EEPROM.put(EEPROM_ADDR_ANCHOR_LAT, _anchorLat);
    EEPROM.put(EEPROM_ADDR_ANCHOR_LNG, _anchorLng);
    EEPROM.commit();
    EEPROM.end();
    Serial.println(F("[EEPROM] Pengaturan Geofence tersimpan ke memori lokal flash."));
}

void SecuritySystem::loadGeofenceFromEEPROM() {
    EEPROM.begin(EEPROM_SIZE);
    uint8_t magic = EEPROM.read(EEPROM_ADDR_MAGIC);
    if (magic == EEPROM_MAGIC_BYTE) {
        _geofenceConfigLoaded = true;
        _geofenceArmed = (EEPROM.read(EEPROM_ADDR_ENABLED) == 1);
        _autoCutoffGeofence = (EEPROM.read(EEPROM_ADDR_AUTOCUTOFF) == 1);
        EEPROM.get(EEPROM_ADDR_RADIUS, _geofenceRadius);
        EEPROM.get(EEPROM_ADDR_ANCHOR_LAT, _anchorLat);
        EEPROM.get(EEPROM_ADDR_ANCHOR_LNG, _anchorLng);

        if (isnan(_geofenceRadius) || _geofenceRadius < 5.0f || _geofenceRadius > 10000.0f) {
            _geofenceRadius = GEOFENCE_DEFAULT_RADIUS;
        }
        if (isnan(_anchorLat) || isnan(_anchorLng)) {
            _anchorLat = 0.0;
            _anchorLng = 0.0;
        }

        Serial.print(F("[EEPROM] Pengaturan Geofence dimuat: Enabled="));
        Serial.print(_geofenceArmed ? F("YA") : F("TIDAK"));
        Serial.print(F(" | Radius="));
        Serial.print(_geofenceRadius, 0);
        Serial.print(F("m | Anchor="));
        Serial.print(_anchorLat, 6);
        Serial.print(F(","));
        Serial.println(_anchorLng, 6);
    } else {
        _geofenceConfigLoaded = false;
        Serial.println(F("[EEPROM] Belum ada data geofence tersimpan. Menggunakan default."));
    }
    EEPROM.end();
}

void SecuritySystem::autoArmGeofenceOnFix(double lat, double lng) {
    if (_geofenceAutoArmed) return;

    // Jika EEPROM sudah mempunyai konfigurasi, hormati pilihan enabled/OFF
    // pengguna. Auto-arm hanya berlaku pada perangkat yang belum dikonfigurasi.
    if (_geofenceConfigLoaded) {
        _geofenceAutoArmed = true;
        Serial.print(F("[GEOFENCE] Konfigurasi EEPROM dipertahankan: "));
        Serial.println(_geofenceArmed ? F("AKTIF") : F("NONAKTIF"));
        return;
    }

    // Jika belum ada titik anchor, gunakan lokasi awal pertama kali GPS fix
    if (_anchorLat == 0.0 && _anchorLng == 0.0) {
        _anchorLat = lat;
        _anchorLng = lng;
    }

    _geofenceArmed = true;
    _geofenceAutoArmed = true;
    saveGeofenceToEEPROM();

    Serial.print(F("[GEOFENCE OFFLINE] Geofence otomatis AKTIF di titik parkir: "));
    Serial.print(_anchorLat, 6);
    Serial.print(F(", "));
    Serial.print(_anchorLng, 6);
    Serial.print(F(" (Radius: "));
    Serial.print(_geofenceRadius, 0);
    Serial.println(F("m)"));
}
