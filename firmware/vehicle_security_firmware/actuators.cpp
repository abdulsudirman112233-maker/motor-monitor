#include "actuators.h"

ActuatorManager::ActuatorManager(uint8_t relayPin, uint8_t buzzerPin)
    : _relayPin(relayPin), _buzzerPin(buzzerPin), _isEngineLocked(false),
      _currentPattern(BUZZER_OFF), _patternStartTime(0), _lastToggleTime(0),
      _stepCount(0), _buzzerState(false) {
}

void ActuatorManager::begin() {
    // Logika Relay Dibalik (Active HIGH / disesuaikan dengan wiring motor):
    // LOW  = Relay Normal (Mesin Menyala / Normal)
    // HIGH = Relay Cut-Off (Mesin Terputus / Dimatikan)
    pinMode(_relayPin, OUTPUT);
    digitalWrite(_relayPin, LOW); // Default: mesin normal / tidak terkunci saat awal boot
    _isEngineLocked = false;

    // Buzzer pin ke basis transistor 2N2222: HIGH = Buzzer ON, LOW = Buzzer OFF
    pinMode(_buzzerPin, OUTPUT);
    digitalWrite(_buzzerPin, LOW);
    _buzzerState = false;

    Serial.println(F("[ACTUATOR] Relay (Kondisi Dibalik) & Buzzer siap dioperasikan."));
}

void ActuatorManager::setEngineLocked(bool lock) {
    _isEngineLocked = lock;
    if (_isEngineLocked) {
        // Cut-off pengapian (Relay HIGH = Jalur pengapian terputus)
        digitalWrite(_relayPin, HIGH);
        Serial.println(F("[ACTUATOR] >>> ENGINE LOCKED / CUT-OFF DIAKTIFKAN (PIN HIGH) <<<"));
    } else {
        // Pulihkan pengapian (Relay LOW = Jalur normal)
        digitalWrite(_relayPin, LOW);
        Serial.println(F("[ACTUATOR] >>> ENGINE UNLOCKED / PENGAPIAN NORMAL (PIN LOW) <<<"));
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
