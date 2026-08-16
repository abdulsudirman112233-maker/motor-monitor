// =============================================================================
// SKETCH DIAGNOSTIK & PENGUJIAN MANDIRI: BUZZER 5V (NODEMCU ESP8266)
// =============================================================================
// Gunakan sketch ini untuk menguji secara fisik apakah Buzzer 5V Anda berbunyi nyaring.
//
// SKEMA SAMBUNGAN:
// A. Jika menggunakan MODUL BUZZER 3-PIN:
//    - Pin VCC ---> Pin VIN (5V) NodeMCU
//    - Pin GND ---> Pin GND NodeMCU
//    - Pin I/O (SIG) ---> Pin D0 (GPIO16) NodeMCU
//
// B. Jika menggunakan BUZZER 2-KAKI (Driver Transistor NPN 2N2222 / BC547):
//    - Buzzer (+) ---> Pin VIN (5V) NodeMCU
//    - Buzzer (-) ---> Kolektor (C) Transistor
//    - Basis (B) Transistor ---> Resistor 1k Ohm ---> Pin D0 NodeMCU
//    - Emitor (E) Transistor ---> Pin GND NodeMCU
// =============================================================================

#define PIN_BUZZER D0 // GPIO16

void beep(uint16_t durationMs) {
    digitalWrite(PIN_BUZZER, HIGH);
    delay(durationMs);
    digitalWrite(PIN_BUZZER, LOW);
}

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println();
    Serial.println(F("================================================="));
    Serial.println(F("     UJI COBA MANDIRI SUARA BUZZER 5V ALARM     "));
    Serial.println(F("================================================="));

    pinMode(PIN_BUZZER, OUTPUT);
    digitalWrite(PIN_BUZZER, LOW);
}

void loop() {
    // 1. Pola 1 Beep Pendek (Arm Chirp / Pre-Warning)
    Serial.println(F("[BUZZER] 1x Beep Pendek (Arm / Pre-Warning)..."));
    beep(150);
    delay(1500);

    // 2. Pola 2 Beep (Disarm Chirp)
    Serial.println(F("[BUZZER] 2x Beep (Disarm System)..."));
    beep(120);
    delay(100);
    beep(120);
    delay(1500);

    // 3. Pola 3 Beep (Cari Motor / Locator)
    Serial.println(F("[BUZZER] 3x Beep (Cari Motor / Locator)..."));
    for (int i = 0; i < 3; i++) {
        beep(100);
        delay(80);
    }
    delay(2000);

    // 4. Pola Sirene Darurat Pencurian (Alarm 3 Detik)
    Serial.println(F("[BUZZER] >>> SIRENE DARURAT PENCURIAN (3 DETIK) <<<"));
    for (int i = 0; i < 10; i++) {
        digitalWrite(PIN_BUZZER, HIGH);
        delay(150);
        digitalWrite(PIN_BUZZER, LOW);
        delay(100);
    }
    delay(3000);
}
