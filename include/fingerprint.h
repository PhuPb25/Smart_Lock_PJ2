#ifndef FINGERPRINT_H
#define FINGERPRINT_H

#include <Arduino.h>
#include <Adafruit_Fingerprint.h>
#include <HardwareSerial.h>

int findFreeSlotAS1();
int findFreeSlotAS2();

uint8_t enrollAS1(uint8_t slot, String name, String code);
uint8_t enrollFinger(String name, String code, uint8_t sensorIdx);

void syncAfterEnroll(uint8_t slot, String name, String code);

uint8_t manualDownloadModelAS(Adafruit_Fingerprint& finger, HardwareSerial& serial, uint8_t* dest);
uint8_t uploadFingerprintTemplate(Adafruit_Fingerprint& finger, HardwareSerial& serial,
                                  uint16_t slot, String base64Data);

void syncFromServer(uint8_t sensorIdx);
void syncUserFromServer(String code, uint8_t sensorIdx);

void checkAS1();
void checkAS2();
void checkFingerprint();

void emptyDatabaseAS1();
void emptyDatabaseAS2();

bool deleteTemplateAS1(uint16_t slot);
bool deleteTemplateAS2(uint16_t slot);

#endif