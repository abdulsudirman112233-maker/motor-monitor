#include "gsm_sim800l.h"

GSMSim800L::GSMSim800L(uint8_t rxPin, uint8_t txPin)
    : _gsmSerial(rxPin, txPin), _hasNewSMS(false), _lastCSQCheck(0) {
    _status.isModuleReady = false;
    _status.isSimRegistered = false;
    _status.csq = 0;
    _status.signalDbm = -115;
    _status.signalPercent = 0;
    _status.operatorName = "SEARCHING";
    _status.isGprsAttached = false;
}

bool GSMSim800L::begin(uint32_t baudRate) {
    _gsmSerial.begin(baudRate);
    Serial.println(F("[GSM] Inisialisasi SoftwareSerial SIM800L..."));
    
    // Kirim sinkronisasi AT berulang kali (Auto-Baud sync)
    bool ok = false;
    for (int i = 0; i < 4; i++) {
        delay(400);
        String resp = sendATCommand("AT", 1000, "OK");
        if (resp.indexOf("OK") != -1) {
            ok = true;
            break;
        }
    }

    if (ok) {
        _status.isModuleReady = true;
        Serial.println(F("[GSM] Modul SIM800L Merespons OK."));
    } else {
        Serial.println(F("[GSM] PERINGATAN: Modul SIM800L Tidak Merespons!"));
        Serial.println(F("[GSM TIPS] 1. Cek Catu Daya 3.8V-4.0V (Min 2A) + Elco 1000uF."));
        Serial.println(F("[GSM TIPS] 2. Pastikan D1(RX ESP)->TXD SIM800L dan D2(TX ESP)->RXD SIM800L."));
        Serial.println(F("[GSM TIPS] 3. Pastikan GND ESP8266 & GND SIM800L terhubung bersama (Common GND)."));
        return false;
    }

    // Matikan echo agar parsing lebih mudah
    sendATCommand("ATE0", 1000, "OK");

    // Aktifkan Extended Error Reporting untuk pesan error detail
    sendATCommand("AT+CMEE=2", 1000, "OK");
    
    // Periksa status kartu SIM
    String pinResp = sendATCommand("AT+CPIN?", 1500, "OK");
    if (pinResp.indexOf("READY") != -1) {
        Serial.println(F("[GSM] Status SIM Card: READY (Terpasang & Siap)."));
    } else {
        Serial.println(F("[GSM] PERINGATAN: SIM Card tidak terdeteksi atau terkunci PIN!"));
    }

    // Set SMS format ke TEXT Mode
    sendATCommand("AT+CMGF=1", 1000, "OK");

    // Set Character Set ke GSM Standard 7-Bit
    sendATCommand("AT+CSCS=\"GSM\"", 1000, "OK");

    // Set SMS Text Mode Parameters (Standard SMS 160 char, 1 day validity)
    sendATCommand("AT+CSMP=17,167,0,0", 1000, "OK");
    
    // Aktifkan indikasi SMS masuk langsung (direct notification CMT)
    sendATCommand("AT+CNMI=2,2,0,0,0", 1000, "OK");

    // Periksa status registrasi jaringan seluler
    String regResp = sendATCommand("AT+CREG?", 2000, "OK");
    if (regResp.indexOf(",1") != -1 || regResp.indexOf(",5") != -1) {
        _status.isSimRegistered = true;
        Serial.println(F("[GSM] SIM Card Terdaftar di Jaringan Seluler."));
    } else {
        Serial.println(F("[GSM] Mencari sinyal jaringan seluler..."));
    }

    // Periksa nama operator
    String copsResp = sendATCommand("AT+COPS?", 3000, "OK");
    int firstQuote = copsResp.indexOf('"');
    if (firstQuote != -1) {
        int secondQuote = copsResp.indexOf('"', firstQuote + 1);
        if (secondQuote != -1) {
            _status.operatorName = copsResp.substring(firstQuote + 1, secondQuote);
            Serial.print(F("[GSM] Operator: "));
            Serial.println(_status.operatorName);
        }
    }

    checkSignalQuality();
    return true;
}

void GSMSim800L::update() {
    _gsmSerial.listen();
    _parseIncomingStream();

    if (millis() - _lastCSQCheck > INTERVAL_GSM_CHECK) {
        _lastCSQCheck = millis();
        if (!_status.isModuleReady) {
            // Coba auto-reconnect jika sebelumnya modul belum siap
            String resp = sendATCommand("AT", 1000, "OK");
            if (resp.indexOf("OK") != -1) {
                begin(GSM_BAUD_RATE);
            }
        } else {
            checkSignalQuality();
        }
    }
}

void GSMSim800L::_parseIncomingStream() {
    while (_gsmSerial.available() > 0) {
        String line = _gsmSerial.readStringUntil('\n');
        line.trim();
        if (line.length() == 0) continue;

        // Cek jika ada notifikasi SMS baru: +CMT: "+6281234567890","","26/08/14,08:50:00+28"
        if (line.startsWith("+CMT:")) {
            _parseCMTLine(line);
        }
    }
}

void GSMSim800L::_parseCMTLine(const String &headerLine) {
    // Format: +CMT: "+6281234567890","","26/08/14,08:50:00+28"
    int firstQuote = headerLine.indexOf('"');
    int secondQuote = headerLine.indexOf('"', firstQuote + 1);
    
    if (firstQuote != -1 && secondQuote != -1) {
        _latestSMS.senderNumber = headerLine.substring(firstQuote + 1, secondQuote);
    }
    
    // Baris berikutnya adalah isi pesan SMS
    unsigned long startWait = millis();
    while (!_gsmSerial.available() && millis() - startWait < 2000) {
        delay(10);
    }
    
    if (_gsmSerial.available()) {
        String text = _gsmSerial.readStringUntil('\n');
        text.trim();
        _latestSMS.messageText = text;
        _latestSMS.isNew = true;
        _hasNewSMS = true;
        
        Serial.print(F("[GSM] SMS MASUK Dari: "));
        Serial.print(_latestSMS.senderNumber);
        Serial.print(F(" | Pesan: "));
        Serial.println(_latestSMS.messageText);
    }
}

bool GSMSim800L::sendSMS(const String &phoneNumber, const String &messageText) {
    Serial.print(F("[GSM] Mengirim SMS ke: "));
    Serial.println(phoneNumber);

    _gsmSerial.listen();
    delay(50);
    
    // 1. Pastikan Text Mode dan Konfigurasi SMS GSM aktif
    sendATCommand("AT+CMGF=1", 800, "OK");
    sendATCommand("AT+CSCS=\"GSM\"", 800, "OK");
    sendATCommand("AT+CSMP=17,167,0,0", 800, "OK");

    // Bersihkan buffer serial
    while (_gsmSerial.available()) {
        _gsmSerial.read();
    }

    // 2. Kirim perintah tujuan nomor HP
    String cmd = "AT+CMGS=\"" + phoneNumber + "\"";
    _gsmSerial.println(cmd);
    
    // 3. Tunggu prompt '>' dari SIM800L
    unsigned long startTime = millis();
    bool gotPrompt = false;
    while (millis() - startTime < 6000) {
        if (_gsmSerial.available()) {
            char c = _gsmSerial.read();
            if (c == '>') {
                gotPrompt = true;
                break;
            }
        }
        delay(5);
    }

    if (!gotPrompt) {
        Serial.println(F("[GSM] Gagal: Tidak mendapatkan prompt '>' dari SIM800L. Cek sinyal & pulsa SIM card!"));
        return false;
    }

    // 4. Kirim teks pesan dan akhiri dengan Ctrl+Z (ASCII 26)
    _gsmSerial.print(messageText);
    delay(200);
    _gsmSerial.write(26); // ASCII 26 = Ctrl+Z
    _gsmSerial.println();
    delay(200);

    // 5. Tunggu respons pengiriman dari jaringan seluler (maksimal 25 detik)
    startTime = millis();
    String resp = "";
    while (millis() - startTime < 25000) {
        if (_gsmSerial.available()) {
            char c = _gsmSerial.read();
            resp += c;
            if (resp.indexOf("+CMGS:") != -1 || resp.indexOf("OK") != -1) {
                Serial.print(F("[GSM] SMS Berhasil Terkirim! Respons: "));
                Serial.println(resp);
                return true;
            }
            if (resp.indexOf("ERROR") != -1 || resp.indexOf("+CMS ERROR") != -1) {
                Serial.print(F("[GSM] Gagal Kirim SMS. Respons Modem: "));
                Serial.println(resp);
                return false;
            }
        }
        delay(10);
    }

    Serial.println(F("[GSM] Timeout: Tidak ada balasan dari jaringan seluler saat mengirim SMS."));
    return false;
}

bool GSMSim800L::checkSignalQuality() {
    String resp = sendATCommand("AT+CSQ", 1500, "OK");
    int idx = resp.indexOf("+CSQ: ");
    if (idx != -1) {
        int commaIdx = resp.indexOf(',', idx);
        if (commaIdx != -1) {
            String csqStr = resp.substring(idx + 6, commaIdx);
            csqStr.trim();
            uint8_t csqVal = csqStr.toInt();
            _status.csq = csqVal;
            
            if (csqVal == 99 || csqVal == 0) {
                _status.signalPercent = 0;
                _status.signalDbm = -115;
            } else {
                _status.signalPercent = map(csqVal, 2, 31, 10, 100);
                _status.signalDbm = -113 + (csqVal * 2);
            }
            return true;
        }
    }
    return false;
}

GSMStatus GSMSim800L::getStatus() const {
    return _status;
}

bool GSMSim800L::hasIncomingSMS() const {
    return _hasNewSMS;
}

SMSMessage GSMSim800L::getLatestSMS() {
    _hasNewSMS = false;
    return _latestSMS;
}

void GSMSim800L::clearIncomingSMS() {
    _hasNewSMS = false;
    _latestSMS.isNew = false;
    _latestSMS.messageText = "";
}

String GSMSim800L::sendATCommand(const String &command, uint32_t timeoutMs, const char* expectedReply) {
    _gsmSerial.listen();
    while (_gsmSerial.available()) {
        _gsmSerial.read(); // Clear buffer
    }

    _gsmSerial.println(command);
    
    String response = "";
    uint32_t startTime = millis();
    while (millis() - startTime < timeoutMs) {
        if (_gsmSerial.available()) {
            char c = _gsmSerial.read();
            response += c;
            if (expectedReply != nullptr && response.indexOf(expectedReply) != -1) {
                break;
            }
        }
    }
    return response;
}

bool GSMSim800L::initGPRS(const String &apn, const String &user, const String &pass) {
    Serial.println(F("[GSM] Menghubungkan GPRS..."));
    sendATCommand("AT+SAPBR=3,1,\"Contype\",\"GPRS\"", 2000, "OK");
    sendATCommand("AT+SAPBR=3,1,\"APN\",\"" + apn + "\"", 2000, "OK");
    
    if (user.length() > 0) {
        sendATCommand("AT+SAPBR=3,1,\"USER\",\"" + user + "\"", 2000, "OK");
    }
    if (pass.length() > 0) {
        sendATCommand("AT+SAPBR=3,1,\"PWD\",\"" + pass + "\"", 2000, "OK");
    }

    // Buka Bearer GPRS
    String resp = sendATCommand("AT+SAPBR=1,1", 8000, "OK");
    if (resp.indexOf("OK") != -1 || resp.indexOf("ALREADY CONNECT") != -1) {
        _status.isGprsAttached = true;
        Serial.println(F("[GSM] GPRS Terhubung Sukses!"));
        return true;
    }
    _status.isGprsAttached = false;
    return false;
}

bool GSMSim800L::isGprsConnected() const {
    return _status.isGprsAttached;
}

void GSMSim800L::closeGPRS() {
    sendATCommand("AT+HTTPTERM", 2000, "OK");
    sendATCommand("AT+SAPBR=0,1", 3000, "OK");
    _status.isGprsAttached = false;
}

bool GSMSim800L::gprsHttpPost(const String &url, const String &jsonData, String &responseOut) {
    return gprsHttpPatch(url, jsonData, responseOut);
}

bool GSMSim800L::gprsHttpPatch(const String &url, const String &jsonData, String &responseOut) {
    if (!_status.isGprsAttached) {
        if (!initGPRS()) return false;
    }

    sendATCommand("AT+HTTPTERM", 1000, "OK");
    sendATCommand("AT+HTTPINIT", 2000, "OK");
    
    // Aktifkan SSL jika URL diawali https://
    if (url.startsWith("https://")) {
        sendATCommand("AT+HTTPSSL=1", 1000, "OK");
    }
    
    sendATCommand("AT+HTTPPARA=\"CID\",1", 2000, "OK");
    sendATCommand("AT+HTTPPARA=\"URL\",\"" + url + "\"", 2000, "OK");
    sendATCommand("AT+HTTPPARA=\"CONTENT\",\"application/json\"", 2000, "OK");

    String dataCmd = "AT+HTTPDATA=" + String(jsonData.length()) + ",5000";
    String promptResp = sendATCommand(dataCmd, 3000, "DOWNLOAD");
    if (promptResp.indexOf("DOWNLOAD") == -1) {
        sendATCommand("AT+HTTPTERM", 1000, "OK");
        return false;
    }

    _gsmSerial.print(jsonData);
    delay(200);

    // Kirim HTTP POST (Action 1)
    String actionResp = sendATCommand("AT+HTTPACTION=1", 12000, "+HTTPACTION:");
    bool success = (actionResp.indexOf(",200,") != -1 || actionResp.indexOf(",204,") != -1);
    
    if (success) {
        responseOut = sendATCommand("AT+HTTPREAD", 3000, "OK");
    } else {
        Serial.print(F("[GSM GPRS] HTTP Action Gagal: "));
        Serial.println(actionResp);
    }

    sendATCommand("AT+HTTPTERM", 1000, "OK");
    return success;
}

bool GSMSim800L::gprsHttpGet(const String &url, String &responseOut) {
    if (!_status.isGprsAttached) {
        if (!initGPRS()) return false;
    }

    sendATCommand("AT+HTTPTERM", 1000, "OK");
    sendATCommand("AT+HTTPINIT", 2000, "OK");
    
    if (url.startsWith("https://")) {
        sendATCommand("AT+HTTPSSL=1", 1000, "OK");
    }

    sendATCommand("AT+HTTPPARA=\"CID\",1", 2000, "OK");
    sendATCommand("AT+HTTPPARA=\"URL\",\"" + url + "\"", 2000, "OK");

    String actionResp = sendATCommand("AT+HTTPACTION=0", 10000, "+HTTPACTION:");
    bool success = (actionResp.indexOf(",200,") != -1);
    
    if (success) {
        responseOut = sendATCommand("AT+HTTPREAD", 3000, "OK");
    }

    sendATCommand("AT+HTTPTERM", 1000, "OK");
    return success;
}
