#ifndef ACTUATORS_H
#define ACTUATORS_H

#include <Arduino.h>
#include "config.h"

enum BuzzerPattern {
    BUZZER_OFF,
    BUZZER_ARM_CHIRP,      // 1 Beep
    BUZZER_DISARM_CHIRP,   // 2 Beeps
    BUZZER_FINDER_CHIRP,   // 3 Beeps
    BUZZER_PANIC_SIREN,    // Continuous alternating siren
    BUZZER_SHORT_ALERT     // Single alert beep
};

class ActuatorManager {
public:
    ActuatorManager(uint8_t relayPin = PIN_RELAY_IGNITION, uint8_t buzzerPin = PIN_BUZZER);
    
    void begin();
    void update();
    
    // Kontrol Relay Pemutus Pengapian (Engine Cut-off)
    void setEngineLocked(bool lock);
    bool isEngineLocked() const;
    
    // Kontrol Buzzer Alarm
    void setBuzzerPattern(BuzzerPattern pattern);
    void triggerArmChirp();
    void triggerDisarmChirp();
    void triggerFinderChirp();
    void triggerPanicSiren();
    void stopBuzzer();
    bool isBuzzerActive() const;

private:
    uint8_t _relayPin;
    uint8_t _buzzerPin;
    bool _isEngineLocked;
    
    BuzzerPattern _currentPattern;
    uint32_t _patternStartTime;
    uint32_t _lastToggleTime;
    uint8_t _stepCount;
    bool _buzzerState;
    
    void _setBuzzerHardware(bool state);
    void _processBuzzerPattern();
};

#endif // ACTUATORS_H
