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
#define R503_RX_PIN   15
#define R503_TX_PIN   16

static void initAS608();
static void initR503();
static void initRFID();
static void initWiFi();
static void initNTP();

// =========================================
// SETUP
// =========================================
void setup() {
    Serial.begin(9600);
    // uint32_t bauds[] = {9600, 19200, 38400, 57600, 115200};
    // bool found = false;

    // for (int i = 0; i < 5; i++) {
    //     Serial.printf("\n[R503] Thử quét với Baudrate: %d...\n", bauds[i]);
        
    //     // Gọi hàm begin của driver để cấu hình lại HardwareSerial theo baudrate thử nghiệm
    //     if (r503.begin(bauds[i], R503_RX_PIN, R503_TX_PIN)) { 
    //         Serial.printf("==> 🔥 TÌM THẤY RỒI! Cảm biến đang sống ở Baudrate: %d\n", bauds[i]);
    //         found = true;
    //         break; // Tìm thấy thì dừng vòng lặp luôn
    //     }
        
    //     delay(200); // Đợi một chút trước khi thử mức baudrate tiếp theo
    // }

    // if (!found) {
    //     Serial.println("\n[R503] ❌ Cảnh báo: Quét hết các mức tốc độ vẫn KHÔNG Handshake được!");
    //     Serial.println("Vui lòng kiểm tra lại dây cáp nguồn, chân RX/TX xem có bị lỏng không.");
    // }

    // prefs.begin phải gọi TRƯỚC initSecrets và loadRFID
    prefs.begin("users", false);

    // Khởi tạo secrets vào flash lần đầu
    initSecrets();

    initAS608();
    initR503();
    initRFID();

    // LED NeoPixel
    pixels.begin();
    pixels.clear();
    pixels.show();

    loadRFID();
    // emptyDatabaseAS();
    // emptyDatabaseRS();

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
        checkAS608();
        checkR503();
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
// INIT R503 — UART2 — 8N2 (R503 yêu cầu 2 stop bits)
// begin() sẽ khởi tạo serial và thực hiện handshake
// =========================================
static void initR503() {
    // begin() tự khởi tạo serial với SERIAL_8N2 và thực hiện handshake
    if (r503.begin(57600, R503_RX_PIN, R503_TX_PIN)) {
        r503.setLED(0x01,0x07);
        //r503.setSecurityLevel(5);
        uint16_t count = 0;
        if (r503.getTemplateCount(&count)) {
            Serial.printf("[R503] Templates stored: %d\n", count);
        }
    } else {
        Serial.println("[R503] Khoi tao DRIVER THAT BAI — kiem tra day noi va baud rate");
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