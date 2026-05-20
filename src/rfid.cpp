#include "rfid.h"
#include "globals.h"
#include "config.h"
#include "lock_control.h"
#include "log.h"
#include <HTTPClient.h>
#include <ArduinoJson.h>

RFIDUser rfidUsers[MAX_RFID];
int rfidCount = 0;

// ===== Load RFID từ Preferences =====
void loadRFID() {
    rfidCount = prefs.getInt("rfid_count", 0);
    for (int i = 0; i < rfidCount; i++) {
        String uidStr = prefs.getString(("rfid_uid_" + String(i)).c_str(), "");
        String name   = prefs.getString(("rfid_name_" + String(i)).c_str(), "Unknown");
        rfidUsers[i].uidStr = uidStr;
        rfidUsers[i].name   = name;
        if (uidStr.length() == 8) {
            for (int j = 0; j < 4; j++) {
                String byteStr = uidStr.substring(j * 2, j * 2 + 2);
                rfidUsers[i].uid[j] = (byte)strtoul(byteStr.c_str(), NULL, 16);
            }
        }
    }
}

// ===== Lưu RFID =====
void saveRFID(String uidStr, String name) {
    if (rfidCount >= MAX_RFID) return;
    prefs.putString(("rfid_uid_"  + String(rfidCount)).c_str(), uidStr);
    prefs.putString(("rfid_name_" + String(rfidCount)).c_str(), name);
    rfidUsers[rfidCount].uidStr = uidStr;
    rfidUsers[rfidCount].name   = name;
    if (uidStr.length() == 8) {
        for (int j = 0; j < 4; j++) {
            String byteStr = uidStr.substring(j * 2, j * 2 + 2);
            rfidUsers[rfidCount].uid[j] = (byte)strtoul(byteStr.c_str(), NULL, 16);
        }
    }
    rfidCount++;
    prefs.putInt("rfid_count", rfidCount);
}

// ===== Tìm thẻ tương ứng =====
static int findRFID(byte* uid) {
    for (int i = 0; i < rfidCount; i++) {
        bool match = true;
        for (int j = 0; j < 4; j++) {
            if (uid[j] != rfidUsers[i].uid[j]) { match = false; break; }
        }
        if (match) return i;
    }
    return -1;
}

// ===== Đẩy log RFID lên server (sensor="RFID") =====
// FIX: đây là lần POST duy nhất cho RFID — không gọi thêm logAccessAS
static void pushRFIDLog(const String& uid, const String& name, bool granted) {
    if (WiFi.status() != WL_CONNECTED) return;

    HTTPClient http;
    http.begin(getServerBase() + "/api/log");
    http.addHeader("Content-Type", "application/json");

    JsonDocument doc;
    doc["slot"]    = 200;
    doc["uid"]     = uid;
    doc["name"]    = name;
    doc["code"]    = uid;
    doc["granted"] = granted;
    doc["sensor"]  = "RFID";

    String payload;
    serializeJson(doc, payload);

    int httpCode = http.POST(payload);
    if (httpCode != 200) {
        Serial.printf("[rfid log] Push server thất bại: HTTP %d\n", httpCode);
    }
    http.end();
}

// ===== Kiểm tra thẻ RFID =====
void checkRFID() {
    if (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial()) return;

    String uidStr = "";
    for (int i = 0; i < rfid.uid.size; i++) {
        if (rfid.uid.uidByte[i] < 0x10) uidStr += "0";
        uidStr += String(rfid.uid.uidByte[i], HEX);
    }
    uidStr.toUpperCase();

    int idx = findRFID(rfid.uid.uidByte);

    if (idx != -1) {
        String name = rfidUsers[idx].name;
        Serial.printf("[RFID] Match! UID: %s | Name: %s\n", uidStr.c_str(), name.c_str());
        // FIX: chỉ gọi pushRFIDLog — không gọi logAccessAS (tránh 2 POST)
        pushRFIDLog(uidStr, name, true);
        openLock();
    } else {
        Serial.printf("[RFID] Không nhận ra UID: %s\n", uidStr.c_str());
        pushRFIDLog(uidStr, "Unknown RFID", false);
        denyAccess();
    }

    rfid.PICC_HaltA();
    rfid.PCD_StopCrypto1();
}