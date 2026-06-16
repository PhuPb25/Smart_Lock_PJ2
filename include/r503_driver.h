#ifndef R503_DRIVER_H
#define R503_DRIVER_H

#include <Arduino.h>
#include <HardwareSerial.h>

class R503 {
public:
    R503(HardwareSerial& serial);

    bool begin(uint32_t baud = 57600, int rxPin = -1, int txPin = -1);
    
    // === CÁC HÀM CHÍNH ===
    bool isTouched();   // Poll nhanh — có ngón tay không? (không blocking)
    bool enroll(uint16_t id);
    bool verify(uint16_t* matchedID = nullptr, uint16_t* score = nullptr);
    
    bool emptyDatabase();
    bool getTemplateCount(uint16_t* count);
    bool setLED(uint8_t mode, uint8_t color);
    bool setSecurityLevel(uint8_t level);
    
    // === TEMPLATE SYNC (Quan trọng cho server sync) ===
    bool downloadTemplate(uint16_t id, uint8_t* buffer, uint16_t* outLength);  // UpChar
    bool uploadTemplate(uint16_t id, const uint8_t* templateData, uint16_t len); // DownChar

    uint8_t getLastError() const { return lastError; }
    bool deleteTemplate(uint16_t id);
    String getErrorMessage(uint8_t code) const;

private:
    HardwareSerial& _serial;
    uint8_t lastError = 0;

    bool sendCommand(uint8_t cmdCode, const uint8_t* params = nullptr, uint16_t paramLen = 0);
    bool readPacket(uint8_t* buf, uint32_t timeout = 4000);
    bool readDataPackets(uint8_t* buffer, uint16_t maxLen, uint16_t* receivedLen);
    uint8_t storeModel(uint16_t id, uint8_t slot);
};

#endif