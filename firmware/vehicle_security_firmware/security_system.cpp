#include "security_system.h"

SecuritySystem::SecuritySystem(uint8_t sw420Pin)
    : _sw420Pin(sw420Pin), _state(SEC_DISARMED), _actuators(nullptr),
      _gps(nullptr), _gsm(nullptr), _vibrationFlag(false),
      _lastSensorState(LOW), _shockCountInWindow(0), _windowStartTime(0),
      _lastPreWarnTime(0), _lastVibrationTime(0), _vibrationCounter(0),
      _alarmTriggeredTime(0), _lastAlarmReason("NONE"), _smsAlertSent(false) {
}

void SecuritySystem::begin(ActuatorManager* actuators, GPSManager* gps, GSMSim800L* gsm) {
    _actuators = actuators;
    _gps = gps;
    _gsm = gsm;

    pinMode(_sw420Pin, INPUT);
    _lastSensorState = digitalRead(_sw420Pin);
    _state = SEC_ARMED; // Default saat dinyalakan: Sistem dalam keadaan siaga/ARMED
    
    Serial.println(F("[SECURITY] Sistem Keamanan Aktif. Mode: ARMED (Sensitivitas Cerdas Anti False-Alarm)."));
}

void SecuritySystem::update() {
    _handleVibrationSensor();

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
    Serial.println(F("[SECURITY] Mode Keamanan diubah: ARMED (Terkunci)."));
    if (_actuators) {
        _actuators->triggerArmChirp();
    }
}

void SecuritySystem::disarm() {
    _state = SEC_DISARMED;
    _lastAlarmReason = "NONE";
    _smsAlertSent = false;
    Serial.println(F("[SECURITY] Mode Keamanan diubah: DISARMED (Terbuka)."));
    if (_actuators) {
        _actuators->stopBuzzer();
        _actuators->triggerDisarmChirp();
    }
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

    String smsText = "[PERINGATAN IoT KENDARAAN]\n";
    smsText += "Getaran mencurigakan terdeteksi!\n";
    smsText += "Lokasi: " + mapsUrl + "\n";
    smsText += "Kecepatan: " + String(spd, 1) + " km/h\n";
    smsText += "Waktu: " + String(millis() / 1000) + "s\n";
    smsText += "Ketik #MATIKAN untuk memutus mesin.";

    _gsm->sendSMS(OWNER_PHONE_NUMBER, smsText);
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
