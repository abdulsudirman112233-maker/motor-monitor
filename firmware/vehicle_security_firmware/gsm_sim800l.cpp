#include "gsm_sim800l.h"

GSMSim800L::GSMSim800L(uint8_t rxPin, uint8_t txPin)
    : _gsmSerial(rxPin, txPin), _modem(_gsmSerial),
      _hasNewSMS(false), _lastCSQCheck(0),
      _lastRegistrationCheck(0),
      _lastSmsSuccess(false), _lastSmsType("NONE"), _smsAttemptCounter(0) {
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
    
    // TinyGSM menangani sinkronisasi modem seperti contoh rujukan ESP32.
    // Baud tetap 9600 karena ESP8266 menggunakan SoftwareSerial, bukan UART2.
    bool ok = false;
    for (int i = 0; i < 4; i++) {
        delay(400);
        _gsmSerial.listen();
        if (_modem.init()) {
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

    // TinyGSM memeriksa registrasi circuit-switched yang diperlukan SMS.
    // Batasi tunggu awal agar GPS/dashboard tidak tertahan. Jika 2G belum siap,
    // update() melanjutkan pemeriksaan tiap 5 detik tanpa blocking panjang.
    if (_modem.waitForNetwork(15000L)) {
        _status.isSimRegistered = true;
        Serial.println(F("[GSM] SIM Card Terdaftar di Jaringan Seluler."));
    } else {
        Serial.println(F("[GSM] Mencari sinyal jaringan seluler..."));
    }

    // Periksa nama operator
    _status.operatorName = _modem.getOperator();
    Serial.print(F("[GSM] Operator: "));
    Serial.println(_status.operatorName);

    checkSignalQuality();
    return true;
}

void GSMSim800L::update() {
    _gsmSerial.listen();
    _parseIncomingStream();

    // Saat belum terdaftar, periksa ulang tiap 5 detik. Sebelumnya status baru
    // diperbarui tiap 30 detik sehingga dashboard lama menampilkan "belum
    // terdaftar" walaupun modem berhasil masuk jaringan beberapa detik kemudian.
    if (!_status.isSimRegistered && millis() - _lastRegistrationCheck >= 5000) {
        _lastRegistrationCheck = millis();
        if (_refreshNetworkRegistration()) {
            Serial.println(F("[GSM/TinyGSM] Registrasi jaringan 2G berhasil."));
            checkSignalQuality();
        } else {
            Serial.println(F("[GSM/TinyGSM] Masih mencari jaringan 2G..."));
        }
    }

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
            _refreshNetworkRegistration();
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

bool GSMSim800L::sendSMS(const String &phoneNumber, const String &messageText, const String &messageType) {
    _smsAttemptCounter++;
    _lastSmsType = messageType;
    _lastSmsSuccess = false;

    Serial.print(F("[GSM] Mengirim SMS ke: "));
    Serial.println(phoneNumber);

    _gsmSerial.listen();
    delay(50);

    if (!_status.isModuleReady) {
        Serial.println(F("[GSM] Gagal: modul SIM800L belum siap."));
        return false;
    }

    // Berikan SoftwareSerial secara eksklusif kepada TinyGSM sepanjang transaksi.
    // GPS akan kembali di-listen oleh GPSManager pada putaran loop berikutnya.
    if (!_modem.testAT(1500L)) {
        _status.isModuleReady = false;
        Serial.println(F("[GSM/TinyGSM] Gagal: modem tidak merespons AT."));
        return false;
    }

    if (!_modem.isNetworkConnected() && !_modem.waitForNetwork(20000L)) {
        _status.isSimRegistered = false;
        Serial.println(F("[GSM] Gagal: SIM belum terdaftar di jaringan operator."));
        return false;
    }
    _status.isSimRegistered = true;

    // TinyGSM mengelola CMGF, CMGS, prompt, Ctrl+Z, timeout dan respons modem.
    _lastSmsSuccess = _modem.sendSMS(phoneNumber, messageText);
    Serial.println(_lastSmsSuccess ?
        F("[GSM/TinyGSM] SMS berhasil disubmit ke jaringan.") :
        F("[GSM/TinyGSM] SMS gagal. Periksa CSQ, pulsa, SMSC dan catu 4V/2A."));
    return _lastSmsSuccess;
}

bool GSMSim800L::_refreshNetworkRegistration() {
    _gsmSerial.listen();
    _status.isSimRegistered = _modem.isNetworkConnected();
    if (_status.isSimRegistered &&
        (_status.operatorName.length() == 0 || _status.operatorName == "SEARCHING")) {
        _status.operatorName = _modem.getOperator();
    }
    return _status.isSimRegistered;
}

bool GSMSim800L::wasLastSmsSuccessful() const { return _lastSmsSuccess; }
String GSMSim800L::getLastSmsType() const { return _lastSmsType; }
uint32_t GSMSim800L::getSmsAttemptCounter() const { return _smsAttemptCounter; }

bool GSMSim800L::checkSignalQuality() {
    _gsmSerial.listen();
    int16_t signal = _modem.getSignalQuality();
    if (signal < 0) return false;
    uint8_t csqVal = (uint8_t)signal;
    _status.csq = csqVal;

    if (csqVal == 99 || csqVal == 0) {
        _status.signalPercent = 0;
        _status.signalDbm = -115;
    } else {
        _status.signalPercent = constrain(map(csqVal, 2, 31, 10, 100), 0, 100);
        _status.signalDbm = -113 + (csqVal * 2);
    }
    return true;
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
        delay(1); // yield ke WiFi/watchdog ESP8266 selama menunggu modem
    }
    return response;
}

bool GSMSim800L::initGPRS(const String &apn, const String &user, const String &pass) {
    Serial.println(F("[GSM] Menghubungkan GPRS..."));
    // Reset bearer lama jika ada sesi menggantung di SIM800L
    sendATCommand("AT+SAPBR=0,1", 1000, "OK");
    delay(50);
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
