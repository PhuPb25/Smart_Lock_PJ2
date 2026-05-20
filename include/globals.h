#pragma once

#include <Arduino.h>
#include <Preferences.h>
#include <WebServer.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <Adafruit_Fingerprint.h>
#include <Adafruit_NeoPixel.h>
#include <MFRC522.h>

// =========================================
// SERIAL & CẢM BIẾN VÂN TAY
// =========================================

// AS608 — UART1
extern HardwareSerial fpSerialAS;
extern Adafruit_Fingerprint fingerAS;

// R558S — UART2 (giao tiếp thô qua UART)
extern HardwareSerial fpSerialRS;

// =========================================
// CÁC THIẾT BỊ KHÁC
// =========================================
extern WebServer server;
extern Preferences prefs;
extern Adafruit_NeoPixel pixels;
extern MFRC522 rfid;

// WiFi credentials (định nghĩa trong config.cpp)
extern const char* ssid;
extern const char* password;

// =========================================
// TRẠNG THÁI TOÀN CỤC
// =========================================
extern bool isEnrollingAS;
extern bool isEnrollingRS;
extern bool isScanningRFID;

// =========================================
// TOUCH PIN — R558S báo có ngón tay
// =========================================
extern volatile bool fingerTouched;
void IRAM_ATTR touchInterruptHandler();

// =========================================
// AUTH HELPER — kiểm tra X-API-Key header
// =========================================
bool checkApiKey();