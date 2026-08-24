// =============================================================================
// SKETCH DIAGNOSTIK MANDIRI: UJI COBA MODUL RELAY 2-CHANNEL (NODEMCU ESP8266)
// =============================================================================
// Gunakan sketch ini untuk menguji secara fisik apakah kedua relay bisa "KLIK" ON/OFF.
//
// PENGKABELAN MODUL RELAY 2-CHANNEL:
// - Pin VCC Relay ---> Pin VIN (5V) NodeMCU (Wajib 5V)
// - Pin GND Relay ---> Pin GND NodeMCU
// - Pin IN1 Relay ---> Pin D3 (GPIO0) NodeMCU [Channel 1: Engine Cut-Off]
// - Pin IN2 Relay ---> Pin D4 (GPIO2) NodeMCU [Channel 2: Cadangan / Klakson / Starter]
// - Jumper Kuning (JD-VCC & VCC): Terpasang
// =============================================================================

#define PIN_RELAY_CH1 D3 // GPIO0 - Channel 1 (Engine Cut-Off)
#define PIN_RELAY_CH2 D4 // GPIO2 - Channel 2 (Cadangan / Aux)

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println();
    Serial.println(F("================================================="));
    Serial.println(F("   UJI COBA MANDIRI BUNYI KLIK RELAY 2-CHANNEL  "));
    Serial.println(F("================================================="));

    pinMode(PIN_RELAY_CH1, OUTPUT);
    pinMode(PIN_RELAY_CH2, OUTPUT);

    // Default: Keduanya Normal / Dibalik (LOW = Standby/Normal, HIGH = Aktif)
    digitalWrite(PIN_RELAY_CH1, LOW);
    digitalWrite(PIN_RELAY_CH2, LOW);
    Serial.println(F("[STATUS] Kedua Relay dalam status default (LOW). Siap memulai pengujian..."));
}

void loop() {
    // -------------------------------------------------------------
    // 1. UJI CHANNEL 1 (IN1 -> D3)
    // -------------------------------------------------------------
    Serial.println(F("\n[TEST CH1] >>> RELAY 1 ON (HIGH) - SEHARUSNYA BUNYI KLIK! <<<"));
    digitalWrite(PIN_RELAY_CH1, HIGH); // ON
    delay(2000);

    Serial.println(F("[TEST CH1] >>> RELAY 1 OFF (LOW) - KLIK! <<<"));
    digitalWrite(PIN_RELAY_CH1, LOW); // OFF
    delay(1000);

    // -------------------------------------------------------------
    // 2. UJI CHANNEL 2 (IN2 -> D4)
    // -------------------------------------------------------------
    Serial.println(F("\n[TEST CH2] >>> RELAY 2 ON (HIGH) - SEHARUSNYA BUNYI KLIK! <<<"));
    digitalWrite(PIN_RELAY_CH2, HIGH); // ON
    delay(2000);

    Serial.println(F("[TEST CH2] >>> RELAY 2 OFF (LOW) - KLIK! <<<"));
    digitalWrite(PIN_RELAY_CH2, LOW); // OFF
    delay(2000);
}
