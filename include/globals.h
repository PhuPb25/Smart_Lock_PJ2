#pragma once

#include <Arduino.h>
#include <Preferences.h>
#include <WebServer.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <Adafruit_Fingerprint.h>
#include <Adafruit_NeoPixel.h>
#include <MFRC522.h>
#include "r503_driver.h"


// =========================================
// SERIAL & CẢM BIẾN VÂN TAY
// =========================================

// AS608 — UART1
extern HardwareSerial fpSerialAS;
extern Adafruit_Fingerprint fingerAS;

// R503 — UART2 (dùng thư viện Adafruit_Fingerprint)
extern HardwareSerial fpSerialRS;
extern R503 r503;

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
// AUTH HELPER — kiểm tra X-API-Key header
// =========================================
bool checkApiKey();