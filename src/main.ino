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
#include "oled.h"         


static void initAS1();
static void initAS2();
static void initRFID();
static void initWiFi();
static void initNTP();
static void updateOledCounts();


// =========================================
// SETUP
// =========================================
void setup() {
    Serial.begin(9600);

    prefs.begin("users", false);
    initSecrets();
    oledSetup();
    
    initAS1();
    initAS2();
    initRFID();
    

    // pixels.begin();
    // pixels.clear();
    // pixels.show();

    loadRFID();

    // prefs.clear();
    // emptyDatabaseAS1();
    // emptyDatabaseAS2();

    pinMode(RELAY_PIN, OUTPUT);
    digitalWrite(RELAY_PIN, LOW);

    initWiFi();
    if (WiFi.status() == WL_CONNECTED) {
        initNTP();
    } else {
        oledShow(OLED_NO_WIFI);
    }
    // Cập nhật số liệu lên OLED lần đầu
    updateOledCounts();            
    oledShow(OLED_IDLE);

    setupWebServer();
    Serial.println("=== He thong san sang ===");
    Serial.println("ESP32 IP: " + WiFi.localIP().toString());
}

// =========================================
// LOOP
// =========================================
void loop() {
    server.handleClient();
    oledLoop();   
    updateOledCounts();

    static unsigned long lastSensorCheck = 0;
    if (!isEnrolling && !isScanningRFID) {
        if (millis() - lastSensorCheck >= 500) {
            lastSensorCheck = millis();
            checkAS1();
            checkAS2();
            checkRFID();
        }
    }
}

// =========================================
// INIT AS608 #1 — UART1
// =========================================
static void initAS1() {
    fpSerialAS1.begin(57600, SERIAL_8N1, AS1_RX_PIN, AS1_TX_PIN);
    fingerAS1.begin(57600);
    if (fingerAS1.verifyPassword()) {
        Serial.println("[AS1] Khoi tao OK");
    } else {
        Serial.println("[AS1] Khoi tao THAT BAI — kiem tra day noi");
    }
}

// =========================================
// INIT AS608 #2 — UART2
// =========================================
static void initAS2() {
    fpSerialAS2.begin(57600, SERIAL_8N1, AS2_RX_PIN, AS2_TX_PIN);
    fingerAS2.begin(57600);
    if (fingerAS2.verifyPassword()) {
        Serial.println("[AS2] Khoi tao OK");
    } else {
        Serial.println("[AS2] Khoi tao THAT BAI — kiem tra day noi");
    }
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
// INIT WIFI
// =========================================
static void initWiFi() {
    Serial.print("Dang ket noi WiFi");
    WiFi.begin(ssid, password);
    unsigned long wifiStart = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - wifiStart < 15000) {
        delay(500); Serial.print(".");
    }
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\nWiFi OK — IP: " + WiFi.localIP().toString());
    } else {
        Serial.println("\nWiFi timeout — chay offline");
    }
}

// =========================================
// INIT NTP — GMT+7
// =========================================
static void initNTP() {
    configTime(7 * 3600, 0, "pool.ntp.org");
    Serial.print("Dang lay thoi gian NTP");
    struct tm timeinfo;
    unsigned long ntpStart = millis();
    while (!getLocalTime(&timeinfo) && millis() - ntpStart < 10000) {
        delay(500); Serial.print(".");
    }
    if (getLocalTime(&timeinfo)) {
        char buf[32];
        strftime(buf, sizeof(buf), "%H:%M:%S %d/%m/%Y", &timeinfo);
        Serial.println("\nNTP OK — " + String(buf));
    } else {
        Serial.println("\nNTP timeout");
    }
}

// =========================================
// CẬP NHẬT SỐ LIỆU LÊN OLED
// Đếm số user trong PrefupdateOledCountserences theo prefix
// =========================================
static void updateOledCounts() {
    int as1 = 0, as2 = 0;
    for (int i = 1; i <= 127; i++) {
        if (prefs.isKey(("as1_user_" + String(i)).c_str())) as1++;
        if (prefs.isKey(("as2_user_" + String(i)).c_str())) as2++;
    }
    oledSetCounts(as1, as2, rfidCount);
}
