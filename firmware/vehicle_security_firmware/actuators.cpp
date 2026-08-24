#include "actuators.h"

ActuatorManager::ActuatorManager(uint8_t relayPin, uint8_t buzzerPin)
    : _relayPin(relayPin), _buzzerPin(buzzerPin), _isEngineLocked(false),
      _currentPattern(BUZZER_OFF), _patternStartTime(0), _lastToggleTime(0),
      _stepCount(0), _buzzerState(false) {
}

void ActuatorManager::begin() {
    // Mode Kontak Pengapian NO (Relay ON saat Normal, Relay OFF saat Matikan Mesin):
    // LOW  = Relay ON  (Koil Aktif / Lampu Nyala -> Mesin Normal Siap Jalan)
    // HIGH = Relay OFF (Koil Mati  / Lampu Mati  -> Mesin Terputus / Cut-Off)
    pinMode(_relayPin, OUTPUT);
    digitalWrite(_relayPin, LOW); // Default: Relay ON (Koil aktif agar mesin bisa hidup)
    _isEngineLocked = false;

    // Buzzer pin ke basis transistor 2N2222: HIGH = Buzzer ON, LOW = Buzzer OFF
    pinMode(_buzzerPin, OUTPUT);
    digitalWrite(_buzzerPin, LOW);
    _buzzerState = false;

    Serial.println(F("[ACTUATOR] Relay (Normal=ON, Matikan=OFF) & Buzzer siap dioperasikan."));
}

void ActuatorManager::setEngineLocked(bool lock) {
    _isEngineLocked = lock;
    if (_isEngineLocked) {
        // MATIKAN MESIN: Matikan daya koil relay (Relay OFF / Pin HIGH)
        digitalWrite(_relayPin, HIGH);
        Serial.println(F("[ACTUATOR] >>> MATIKAN MESIN: RELAY MENJADI OFF (LAMPU RELAY MATI) <<<"));
    } else {
        // RESTORE / HIDUPKAN MESIN: Aktifkan koil relay (Relay ON / Pin LOW)
        digitalWrite(_relayPin, LOW);
        Serial.println(F("[ACTUATOR] >>> RESTORE MESIN: RELAY MENJADI ON (LAMPU RELAY NYALA) <<<"));
    }
}

bool ActuatorManager::isEngineLocked() const {
    return _isEngineLocked;
}

void ActuatorManager::_setBuzzerHardware(bool state) {
    _buzzerState = state;
    digitalWrite(_buzzerPin, state ? HIGH : LOW);
}

void ActuatorManager::setBuzzerPattern(BuzzerPattern pattern) {
    _currentPattern = pattern;
    _patternStartTime = millis();
    _lastToggleTime = millis();
    _stepCount = 0;

    if (pattern == BUZZER_OFF) {
        _setBuzzerHardware(false);
    } else {
        _setBuzzerHardware(true);
    }
}

void ActuatorManager::triggerArmChirp() {
    setBuzzerPattern(BUZZER_ARM_CHIRP);
}

void ActuatorManager::triggerDisarmChirp() {
    setBuzzerPattern(BUZZER_DISARM_CHIRP);
}

void ActuatorManager::triggerFinderChirp() {
    setBuzzerPattern(BUZZER_FINDER_CHIRP);
}

void ActuatorManager::triggerPanicSiren() {
    setBuzzerPattern(BUZZER_PANIC_SIREN);
}

void ActuatorManager::stopBuzzer() {
    setBuzzerPattern(BUZZER_OFF);
}

bool ActuatorManager::isBuzzerActive() const {
    return _currentPattern != BUZZER_OFF;
}

void ActuatorManager::update() {
    _processBuzzerPattern();
}

void ActuatorManager::_processBuzzerPattern() {
    if (_currentPattern == BUZZER_OFF) return;

    uint32_t now = millis();
    uint32_t elapsed = now - _lastToggleTime;

    switch (_currentPattern) {
        case BUZZER_ARM_CHIRP:
            // 1 Beep: 150ms ON lalu OFF
            if (_buzzerState && elapsed >= 150) {
                _setBuzzerHardware(false);
                _currentPattern = BUZZER_OFF;
            }
            break;

        case BUZZER_DISARM_CHIRP:
            // 2 Beeps: ON 80ms -> OFF 80ms -> ON 80ms -> OFF
            if (_buzzerState && elapsed >= 80) {
                _setBuzzerHardware(false);
                _lastToggleTime = now;
                _stepCount++;
            } else if (!_buzzerState && elapsed >= 80) {
                if (_stepCount < 2) {
                    _setBuzzerHardware(true);
                    _lastToggleTime = now;
                } else {
                    _currentPattern = BUZZER_OFF;
                }
            }
            break;

        case BUZZER_FINDER_CHIRP:
            // 3 Beeps agak panjang: ON 120ms, OFF 100ms x3
            if (_buzzerState && elapsed >= 120) {
                _setBuzzerHardware(false);
                _lastToggleTime = now;
                _stepCount++;
            } else if (!_buzzerState && elapsed >= 100) {
                if (_stepCount < 3) {
                    _setBuzzerHardware(true);
                    _lastToggleTime = now;
                } else {
                    _currentPattern = BUZZER_OFF;
                }
            }
            break;

        case BUZZER_PANIC_SIREN:
            // Sirene Darurat: ON 200ms, OFF 100ms berulang-ulang
            if (_buzzerState && elapsed >= 200) {
                _setBuzzerHardware(false);
                _lastToggleTime = now;
            } else if (!_buzzerState && elapsed >= 100) {
                _setBuzzerHardware(true);
                _lastToggleTime = now;
            }
            break;

        case BUZZER_SHORT_ALERT:
            if (_buzzerState && elapsed >= 300) {
                _setBuzzerHardware(false);
                _currentPattern = BUZZER_OFF;
            }
            break;

        default:
            _setBuzzerHardware(false);
            _currentPattern = BUZZER_OFF;
            break;
    }
}
