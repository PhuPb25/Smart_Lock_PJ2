#ifndef FINGERPRINT_H
#define FINGERPRINT_H

#include <Arduino.h>

// AS608 vẫn dùng Adafruit
int findFreeSlotAS();
int findFreeSlotRS();

uint8_t enrollAS608(uint8_t slot, String name, String code);
uint8_t enrollR503(uint8_t slot, String name, String code);  // Dùng R503 driver
uint8_t enrollFinger(String name, String code, uint8_t sensorType);

void syncAfterEnrollAS(uint8_t slot, String name, String code);
void syncAfterEnrollRS(uint8_t slot, String name, String code);

uint8_t manualDownloadModelAS(uint8_t* dest);
uint8_t manualDownloadModelRS(uint8_t* dest);   // Sẽ dùng R503 driver

uint8_t uploadFingerprintTemplateAS(uint16_t slot, String base64Data);
uint8_t uploadFingerprintTemplateRS(uint16_t slot, String base64Data);

void syncFromServer(uint8_t sensorType);
void syncUserFromServer(String code, uint8_t sensorType);

void checkAS608();
void checkR503();
void checkFingerprint();

void emptyDatabaseRS();
void emptyDatabaseAS();
bool deleteR503Template(uint16_t slot);

#endif
