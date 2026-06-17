#include "globals.h"
#include "config.h"

// =========================================
// WEB SERVER & STORAGE
// =========================================
WebServer server(80);
Preferences prefs;

// =========================================
// AS608 #1 — UART1 (RX=17, TX=18)
// =========================================
HardwareSerial fpSerialAS1(1);
Adafruit_Fingerprint fingerAS1(&fpSerialAS1);

// =========================================
// AS608 #2 — UART2 (RX=15, TX=16)
// =========================================
HardwareSerial fpSerialAS2(2);
Adafruit_Fingerprint fingerAS2(&fpSerialAS2);

// =========================================
// LED & RFID
// =========================================
Adafruit_NeoPixel pixels(1, 48, NEO_GRB + NEO_KHZ800);
MFRC522 rfid(SS_PIN, RST_PIN);

// =========================================
// TRẠNG THÁI
// =========================================
bool isEnrolling   = false;
bool isScanningRFID = false;

// =========================================
// AUTH
// =========================================
bool checkApiKey() {
    String storedKey = prefs.getString("api_key", "");
    if (storedKey.length() == 0) {
        Serial.println("[auth] CẢNH BÁO: api_key chưa thiết lập!");
        return true;
    }
    String sentKey = server.header("X-API-Key");
    if (sentKey == storedKey) return true;
    server.send(401, "application/json", "{\"error\":\"Unauthorized\"}");
    return false;
}