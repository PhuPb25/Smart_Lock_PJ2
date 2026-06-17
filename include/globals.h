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
// CẢM BIẾN VÂN TAY — 2x AS608
// =========================================
// AS608 #1 — UART1 (RX=17, TX=18)
extern HardwareSerial fpSerialAS1;
extern Adafruit_Fingerprint fingerAS1;

// AS608 #2 — UART2 (RX=15, TX=16)
extern HardwareSerial fpSerialAS2;
extern Adafruit_Fingerprint fingerAS2;

// =========================================
// CÁC THIẾT BỊ KHÁC
// =========================================
extern WebServer server;
extern Preferences prefs;
extern Adafruit_NeoPixel pixels;
extern MFRC522 rfid;

// WiFi credentials
extern const char* ssid;
extern const char* password;

// =========================================
// TRẠNG THÁI TOÀN CỤC
// =========================================
extern bool isEnrolling;
extern bool isScanningRFID;

// =========================================
// AUTH HELPER
// =========================================
bool checkApiKey();