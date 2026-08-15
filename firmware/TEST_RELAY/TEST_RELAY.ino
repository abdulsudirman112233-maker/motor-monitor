// =============================================================================
// SKETCH DIAGNOSTIK MANDIRI: UJI COBA MODUL RELAY 1-CHANNEL (NODEMCU ESP8266)
// =============================================================================
// Gunakan sketch ini untuk menguji secara fisik apakah relay Anda bisa "KLIK" ON/OFF.
//
// PENGKABELAN:
// - Pin VCC Relay ---> Pin VIN (5V) NodeMCU (Wajib 5V)
// - Pin GND Relay ---> Pin GND NodeMCU
// - Pin IN  Relay ---> Pin D3 (GPIO0) NodeMCU
// =============================================================================

#define PIN_RELAY D3 // GPIO0

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println();
    Serial.println(F("================================================="));
    Serial.println(F("   UJI COBA MANDIRI BUNYI KLIK RELAY 1-CHANNEL  "));
    Serial.println(F("================================================="));

    pinMode(PIN_RELAY, OUTPUT);
    digitalWrite(PIN_RELAY, HIGH); // Default: OFF
}

void loop() {
    // 1. AKTIFKAN RELAY (Memicu Bunyi KLIK & Lampu Indikator Relay Menyala)
    Serial.println(F("[TEST] >>> RELAY ON (LOW) - SEHARUSNYA BUNYI KLIK! <<<"));
    digitalWrite(PIN_RELAY, LOW); // Active LOW: 0V mengaktifkan koil relay
    delay(2500); // Tahan 2.5 detik

    // 2. MATIKAN RELAY (Memicu Bunyi KLIK & Lampu Indikator Relay Mati)
    Serial.println(F("[TEST] >>> RELAY OFF (HIGH) - SEHARUSNYA BUNYI KLIK! <<<"));
    digitalWrite(PIN_RELAY, HIGH); // 3.3V menonaktifkan koil relay
    delay(2500); // Tahan 2.5 detik
}
