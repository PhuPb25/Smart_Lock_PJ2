#ifndef FINGERPRINT_H
#define FINGERPRINT_H

#include <Arduino.h>

int findFreeSlotAS();
int findFreeSlotRS();

uint8_t enrollAS608(uint8_t slot, String name, String code);
uint8_t enrollR558S(uint8_t slot, String name, String code);
uint8_t enrollFinger(String name, String code, uint8_t sensorType);

void syncAfterEnrollAS(uint8_t slot, String name, String code);
void syncAfterEnrollRS(uint8_t slot, String name, String code);

uint8_t manualDownloadModelAS(uint8_t* dest);
uint8_t manualDownloadModelRS(uint16_t slot, uint8_t* dest);

uint8_t uploadFingerprintTemplateAS(uint16_t slot, String base64Data);
uint8_t uploadFingerprintTemplateRS(uint8_t slot, String base64Data);

void syncFromServer(uint8_t sensorType);
void syncUserFromServer(String code, uint8_t sensorType);

void checkAS608();
void checkR558S();
void checkFingerprint();

void emptyR558S();
void emptyDatabaseAS();
void r558s_readSysPara();

#endif