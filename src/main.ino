#include "config.h"
#include "globals.h"
#include "lock_control.h"
#include "fingerprint.h"
#include "rfid.h"
#include "web_server.h"
#include "secrets.h"

#include <Arduino.h>
#include <SPI.h>
#include <time.h>

// =========================================
// PIN CẤU HÌNH
// Kiểm tra lại sơ đồ mạch trước khi nạp!
// =========================================
#define AS608_RX_PIN  17
#define AS608_TX_PIN  18
#define R558S_RX_PIN  15
#define R558S_TX_PIN  16
#define TOUCH_PIN     7    // ngắt báo có ngón tay trên R558S

static void initAS608();
static void initR558S();
static void initRFID();
static void initWiFi();
static void initNTP();

// =========================================
// SETUP
// =========================================
void setup() {
    Serial.begin(9600);

    // FIX: prefs.begin phải gọi TRƯỚC initSecrets và loadRFID
    prefs.begin("users", false);

    // Khởi tạo secrets vào flash lần đầu
    initSecrets();

    initAS608();
    initR558S();
    r558s_readSysPara();
    initRFID();

    // TOUCH PIN — R558S kéo xuống LOW khi có ngón tay
    pinMode(TOUCH_PIN, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(TOUCH_PIN),
                    touchInterruptHandler, FALLING);
    Serial.println("[R558S] Touch interrupt OK");

    // LED NeoPixel
    pixels.begin();
    pixels.clear();
    pixels.show();

    loadRFID();
    // emptyDatabaseAS();
    // emptyR558S();

    // Relay
    pinMode(RELAY_PIN, OUTPUT);
    digitalWrite(RELAY_PIN, LOW);

    initWiFi();

    if (WiFi.status() == WL_CONNECTED) {
        initNTP();
    }

    setupWebServer();
    Serial.println("=== He thong san sang ===");
    Serial.println("ESP32 IP: " + WiFi.localIP().toString());
}

// =========================================
// LOOP
// =========================================
void loop() {
    server.handleClient();

    if (!isEnrollingAS && !isEnrollingRS && !isScanningRFID) {

        // AS608 — poll bình thường
        checkAS608();

        // R558S — chỉ gọi khi có ngón tay chạm (qua interrupt)
        if (fingerTouched) {
            detachInterrupt(digitalPinToInterrupt(TOUCH_PIN));
            checkR558S();
            fingerTouched = false;
            attachInterrupt(digitalPinToInterrupt(TOUCH_PIN),
                            touchInterruptHandler, FALLING);
        }

        checkRFID();
    }

    delay(50);
}

// =========================================
// INIT AS608 — UART1 — 8N1
// =========================================
static void initAS608() {
    fpSerialAS.begin(57600, SERIAL_8N1, AS608_RX_PIN, AS608_TX_PIN);
    fingerAS.begin(57600);

    if (fingerAS.verifyPassword()) {
        Serial.println("[AS608] Khoi tao OK");
    } else {
        Serial.println("[AS608] Khoi tao THAT BAI — kiem tra day noi");
    }
}

// =========================================
// INIT R558S — UART2 — 8N2 (bắt buộc!)
// =========================================
static void initR558S() {
    fpSerialRS.begin(57600, SERIAL_8N2, R558S_RX_PIN, R558S_TX_PIN);
    delay(100);

    uint8_t buf[32];
    uint8_t hsCmd[] = {
        0xEF,0x01, 0xFF,0xFF,0xFF,0xFF,
        0x01, 0x00,0x03,
        0x35,
        0x00,0x39
    };
    while (fpSerialRS.available()) fpSerialRS.read();
    fpSerialRS.write(hsCmd, sizeof(hsCmd));

    uint32_t t = millis();
    int idx = 0;
    bool ok = false;
    while (millis() - t < 1000) {
        if (fpSerialRS.available()) {
            uint8_t c = fpSerialRS.read();
            if (idx == 0 && c != 0xEF) continue;
            if (idx == 1 && c != 0x01) { idx = 0; continue; }
            buf[idx++] = c;
            if (idx >= 12) { ok = (buf[9] == 0x00); break; }
        }
    }

    if (ok) {
        Serial.println("[R558S] Khoi tao OK");
    } else {
        Serial.println("[R558S] Khoi tao THAT BAI — kiem tra day noi va SERIAL_8N2");
    }

    // Tắt LED về mặc định
    uint8_t ledOff[] = {
        0xEF,0x01, 0xFF,0xFF,0xFF,0xFF,
        0x01, 0x00,0x07,
        0x3C, 0x04,0x00,0x00,0x00,
        0x00,0x48
    };
    fpSerialRS.write(ledOff, sizeof(ledOff));
}

// =========================================
// INIT RFID
// =========================================
static void initRFID() {
    SPI.begin();
    rfid.PCD_Init();
    Serial.println("[RFID] Khoi tao OK");
}

// =========================================
// INIT WIFI — có timeout, không treo mãi
// =========================================
static void initWiFi() {
    Serial.print("Dang ket noi WiFi");
    WiFi.begin(ssid, password);

    unsigned long wifiStart = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - wifiStart < 15000) {
        delay(500);
        Serial.print(".");
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\nWiFi OK — IP: " + WiFi.localIP().toString());
    } else {
        Serial.println("\nWiFi timeout — chay offline");
    }
}

// =========================================
// INIT NTP — GMT+7 Viet Nam
// =========================================
static void initNTP() {
    configTime(7 * 3600, 0, "pool.ntp.org");
    Serial.print("Dang lay thoi gian NTP");

    struct tm timeinfo;
    unsigned long ntpStart = millis();
    while (!getLocalTime(&timeinfo) && millis() - ntpStart < 10000) {
        delay(500);
        Serial.print(".");
    }

    if (getLocalTime(&timeinfo)) {
        char buf[32];
        strftime(buf, sizeof(buf), "%H:%M:%S %d/%m/%Y", &timeinfo);
        Serial.println("\nNTP OK — " + String(buf));
    } else {
        Serial.println("\nNTP timeout — timestamp log co the sai");
    }
}
