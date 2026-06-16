#include "globals.h"
#include "config.h"

// =========================================
// WEB SERVER & STORAGE
// =========================================
WebServer server(80);
Preferences prefs;

// =========================================
// AS608 — UART1 (RX=17, TX=18)
// =========================================
HardwareSerial fpSerialAS(1);
Adafruit_Fingerprint fingerAS(&fpSerialAS);

// =========================================
// R503 — UART2 (RX=15, TX=16)
// Thư viện Adafruit_Fingerprint hỗ trợ R503 sẵn
// =========================================
HardwareSerial fpSerialRS(2);
R503 r503(fpSerialRS);

// =========================================
// LED & RFID
// =========================================
Adafruit_NeoPixel pixels(1, 48, NEO_GRB + NEO_KHZ800);
MFRC522 rfid(SS_PIN, RST_PIN);

// =========================================
// TRẠNG THÁI
// =========================================
bool isEnrollingAS  = false;
bool isEnrollingRS  = false;
bool isScanningRFID = false;

// =========================================
// Kiểm tra X-API-Key header
// Trả về true nếu hợp lệ, false (và tự gửi 401) nếu không.
// =========================================
bool checkApiKey() {
    String storedKey = prefs.getString("api_key", "");
    if (storedKey.length() == 0) {
        Serial.println("[auth] CẢNH BÁO: api_key chưa được thiết lập!");
        return true;
    }
    String sentKey = server.header("X-API-Key");
    if (sentKey == storedKey) return true;
    server.send(401, "application/json", "{\"error\":\"Unauthorized\"}");
    return false;
}