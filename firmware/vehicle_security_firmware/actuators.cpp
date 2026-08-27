#include "actuators.h"

ActuatorManager::ActuatorManager(uint8_t relayPin, uint8_t buzzerPin)
    : _relayPin(relayPin), _buzzerPin(buzzerPin), _isEngineLocked(false),
      _currentPattern(BUZZER_OFF), _patternStartTime(0), _lastToggleTime(0),
      _stepCount(0), _buzzerState(false) {
}

void ActuatorManager::begin() {
    // Fail-safe COM-NC, modul relay active-low:
    // HIGH = koil relay OFF, COM-NC tersambung, mesin normal.
    // LOW  = koil relay ON, COM-NC terbuka, mesin cut-off.
    // Isi output latch sebelum pinMode untuk meminimalkan pulsa saat startup.
    // Pulihkan keadaan terakhir sebelum pin dijadikan OUTPUT. Ini mencegah
    // pulsa HIGH -> LOW setiap ESP8266 restart ketika mesin sedang di-cutoff.
    EEPROM.begin(64);
    const bool savedStateValid = EEPROM.read(32) == 0xC7;
    _isEngineLocked = savedStateValid && EEPROM.read(33) == 1;
    EEPROM.end();
    const uint8_t startupLevel = _isEngineLocked ?
                                 RELAY_LEVEL_ENGINE_CUTOFF :
                                 RELAY_LEVEL_ENGINE_NORMAL;
    digitalWrite(_relayPin, startupLevel);
    pinMode(_relayPin, OUTPUT);
    digitalWrite(_relayPin, startupLevel);

    // Buzzer pin ke basis transistor 2N2222: HIGH = Buzzer ON, LOW = Buzzer OFF
    digitalWrite(_buzzerPin, LOW);
    pinMode(_buzzerPin, OUTPUT);
    digitalWrite(_buzzerPin, LOW);
    _buzzerState = false;

    Serial.println(F("[ACTUATOR] Relay D0 fail-safe NC & buzzer D8 siap dioperasikan."));
}

void ActuatorManager::setEngineLocked(bool lock) {
    const bool changed = (_isEngineLocked != lock);
    _isEngineLocked = lock;
    if (_isEngineLocked) {
        // MATIKAN MESIN: aktifkan koil sehingga kontak NC terputus.
        digitalWrite(_relayPin, RELAY_LEVEL_ENGINE_CUTOFF);
        Serial.println(F("[ACTUATOR] >>> MATIKAN MESIN: D0 LOW, KOIL RELAY AKTIF, NC TERPUTUS <<<"));
    } else {
        // RESTORE: matikan koil sehingga COM-NC tersambung secara mekanis.
        digitalWrite(_relayPin, RELAY_LEVEL_ENGINE_NORMAL);
        Serial.println(F("[ACTUATOR] >>> RESTORE MESIN: D0 HIGH, KOIL RELAY MATI, NC TERSAMBUNG <<<"));
    }
    if (changed) _saveEngineLockState();
}

void ActuatorManager::_saveEngineLockState() {
    // Ditulis hanya ketika state berubah, bukan di setiap loop, agar flash awet.
    EEPROM.begin(64);
    EEPROM.write(32, 0xC7);
    EEPROM.write(33, _isEngineLocked ? 1 : 0);
    EEPROM.commit();
    EEPROM.end();
}

bool ActuatorManager::isEngineLocked() const {
    return _isEngineLocked;
}

uint8_t ActuatorManager::getRelayOutputLevel() const {
    return digitalRead(_relayPin);
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
