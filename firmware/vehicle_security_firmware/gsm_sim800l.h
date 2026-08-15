#ifndef GSM_SIM800L_H
#define GSM_SIM800L_H

#include <Arduino.h>
#include <SoftwareSerial.h>
#include "config.h"

struct GSMStatus {
    bool isModuleReady;
    bool isSimRegistered;
    uint8_t csq;              // 0 - 31, 99 = unknown
    int16_t signalDbm;        // -113 dBm to -51 dBm
    uint8_t signalPercent;    // 0 - 100%
    String operatorName;
    bool isGprsAttached;
};

struct SMSMessage {
    String senderNumber;
    String dateTime;
    String messageText;
    bool isNew;
};

class GSMSim800L {
public:
    GSMSim800L(uint8_t rxPin, uint8_t txPin);
    
    bool begin(uint32_t baudRate = GSM_BAUD_RATE);
    void update();
    
    bool sendSMS(const String &phoneNumber, const String &messageText);
    bool checkSignalQuality();
    GSMStatus getStatus() const;
    
    // Incoming SMS Callback
    bool hasIncomingSMS() const;
    SMSMessage getLatestSMS();
    void clearIncomingSMS();
    
    // GPRS HTTP Helpers (Fallback if WiFi unavailable)
    bool initGPRS(const String &apn = GSM_APN, const String &user = GSM_APN_USER, const String &pass = GSM_APN_PASS);
    bool isGprsConnected() const;
    bool gprsHttpPost(const String &url, const String &jsonData, String &responseOut);
    bool gprsHttpPatch(const String &url, const String &jsonData, String &responseOut);
    bool gprsHttpGet(const String &url, String &responseOut);
    void closeGPRS();

    String sendATCommand(const String &command, uint32_t timeoutMs = 2000, const char* expectedReply = "OK");

private:
    SoftwareSerial _gsmSerial;
    GSMStatus _status;
    SMSMessage _latestSMS;
    bool _hasNewSMS;
    uint32_t _lastCSQCheck;
    
    void _parseIncomingStream();
    void _parseCMTLine(const String &line);
};

#endif // GSM_SIM800L_H
