// =============================================================================
// SKETCH DIAGNOSTIK & PENGUJIAN SMS SIM800L KE HP PEMILIK (+6281523842859)
// =============================================================================
// Pin SoftwareSerial NodeMCU ESP8266:
// - RX ESP (D1) <--- TXD Modul SIM800L
// - TX ESP (D2) ---> RXD Modul SIM800L
// - Power: VCC SIM800L ke Stepdown 4.0V (Min 2A) / GND bersama
// =============================================================================

#include <SoftwareSerial.h>

#define PIN_GSM_RX D1 // GPIO5  (Terhubung ke TXD SIM800L)
#define PIN_GSM_TX D2 // GPIO4  (Terhubung ke RXD SIM800L)

SoftwareSerial sim800(PIN_GSM_RX, PIN_GSM_TX);

const char* TARGET_PHONE_1 = "+6281523842859";
const char* TARGET_PHONE_2 = "081523842859";

String sendCmd(const String &cmd, uint32_t timeoutMs = 2000) {
    while (sim800.available()) sim800.read();
    sim800.println(cmd);
    
    String response = "";
    uint32_t start = millis();
    while (millis() - start < timeoutMs) {
        if (sim800.available()) {
            char c = sim800.read();
            response += c;
        }
        delay(2);
    }
    Serial.println(F(">> "));
    Serial.println(response);
    return response;
}

bool testSendSMS(const String &phone, const String &msg) {
    Serial.print(F("\n[TEST SMS] Mencoba kirim ke nomor: "));
    Serial.println(phone);

    // 1. Set SMS Text Mode
    sendCmd("AT+CMGF=1", 1000);
    sendCmd("AT+CSCS=\"GSM\"", 1000);
    sendCmd("AT+CSMP=17,167,0,0", 1000);

    // 2. Cek SMS Center
    sendCmd("AT+CSCA?", 1000);

    // Bersihkan buffer
    while (sim800.available()) sim800.read();

    // 3. Kirim AT+CMGS
    String cmgsCmd = "AT+CMGS=\"" + phone + "\"";
    sim800.println(cmgsCmd);

    // 4. Tunggu prompt '>'
    uint32_t start = millis();
    bool gotPrompt = false;
    String promptResp = "";
    while (millis() - start < 5000) {
        if (sim800.available()) {
            char c = sim800.read();
            promptResp += c;
            if (c == '>') {
                gotPrompt = true;
                break;
            }
        }
        delay(5);
    }

    if (!gotPrompt) {
        Serial.print(F("[GAGAL] Tidak dapat prompt '>'. Respon modem: "));
        Serial.println(promptResp);
        return false;
    }

    Serial.println(F("[PROMPT > DIDAPAT] Mengirim isi teks & Ctrl+Z..."));

    // 5. Kirim isi pesan & Ctrl+Z (ASCII 26)
    sim800.print(msg);
    delay(200);
    sim800.write(26);
    sim800.println();
    delay(200);

    // 6. Tunggu konfirmasi kirim dari tower BTS
    start = millis();
    String sendResult = "";
    while (millis() - start < 20000) {
        if (sim800.available()) {
            char c = sim800.read();
            sendResult += c;
            if (sendResult.indexOf("+CMGS:") != -1 || sendResult.indexOf("OK") != -1) {
                Serial.println(F("\n🎉 [SUKSES BESAR] SMS BERHASIL TERKIRIM KE HP PEMILIK!"));
                Serial.println(sendResult);
                return true;
            }
            if (sendResult.indexOf("ERROR") != -1 || sendResult.indexOf("+CMS ERROR") != -1) {
                Serial.println(F("\n❌ [GAGAL] Modem menolak kirim SMS. Respon:"));
                Serial.println(sendResult);
                return false;
            }
        }
        delay(10);
    }

    Serial.println(F("\n⚠️ [TIMEOUT] Tidak ada balasan dari jaringan operator setelah 20 detik."));
    return false;
}

void setup() {
    Serial.begin(115200);
    sim800.begin(9600);
    delay(1500);

    Serial.println();
    Serial.println(F("================================================================"));
    Serial.println(F("    DIAGNOSTIK LENGKAP & PENGUJIAN SMS SIM800L (INDOSAT)        "));
    Serial.println(F("================================================================"));

    // 1. Tes Komunikasi AT
    Serial.println(F("\n1. Tes Komunikasi AT:"));
    sendCmd("AT", 1000);

    // 2. Cek Status SIM Card
    Serial.println(F("\n2. Cek Status SIM Card (CPIN):"));
    sendCmd("AT+CPIN?", 1500);

    // 3. Cek Registrasi Jaringan Seluler
    Serial.println(F("\n3. Cek Registrasi Jaringan (CREG):"));
    sendCmd("AT+CREG?", 2000);

    // 4. Cek Nama Operator
    Serial.println(F("\n4. Cek Operator Jaringan (COPS):"));
    sendCmd("AT+COPS?", 3000);

    // 5. Cek Kualitas Sinyal
    Serial.println(F("\n5. Cek Kualitas Sinyal (CSQ):"));
    sendCmd("AT+CSQ", 1500);

    // 6. Uji Coba Kirim SMS
    String testMsg = "[TEST IoT MOTOR]\nSistem Keamanan Motor IoT Aktif!\nModul SIM800L Normal & Terkoneksi.\nNomor Pemilik: +6281523842859";
    
    bool ok = testSendSMS(TARGET_PHONE_1, testMsg);
    if (!ok) {
        Serial.println(F("\nMencoba format nomor kedua (081523842859)..."));
        testSendSMS(TARGET_PHONE_2, testMsg);
    }

    Serial.println(F("\n================================================================"));
    Serial.println(F("Diagnostik Selesai. Periksa Serial Monitor & SMS di HP Anda."));
    Serial.println(F("================================================================"));
}

void loop() {
    // Teruskan data serial dua arah jika ingin kirim AT Command manual lewat Serial Monitor
    if (sim800.available()) {
        Serial.write(sim800.read());
    }
    if (Serial.available()) {
        sim800.write(Serial.read());
    }
}
