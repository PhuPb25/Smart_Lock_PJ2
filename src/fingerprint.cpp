#include "fingerprint.h"
#include "globals.h"
#include "config.h"
#include "lock_control.h"
#include "log.h"
#include <HTTPClient.h>
#include <ArduinoJson.h>

// =========================================
// BASE64
// =========================================
static const char b64chars[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static String base64Encode(const uint8_t* data, size_t len) {
    String result = "";
    result.reserve((len / 3 + 1) * 4 + 4);
    for (size_t i = 0; i < len; i += 3) {
        uint8_t b0 = data[i];
        uint8_t b1 = (i + 1 < len) ? data[i + 1] : 0;
        uint8_t b2 = (i + 2 < len) ? data[i + 2] : 0;
        result += b64chars[b0 >> 2];
        result += b64chars[((b0 & 0x03) << 4) | (b1 >> 4)];
        result += (i + 1 < len) ? b64chars[((b1 & 0x0F) << 2) | (b2 >> 6)] : '=';
        result += (i + 2 < len) ? b64chars[b2 & 0x3F] : '=';
    }
    return result;
}

static int base64Decode(const String& input, uint8_t* output, size_t maxLen) {
    static const int8_t b64index[256] = {
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,62,-1,-1,-1,63,
        52,53,54,55,56,57,58,59,60,61,-1,-1,-1,-1,-1,-1,
        -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,
        15,16,17,18,19,20,21,22,23,24,25,-1,-1,-1,-1,-1,
        -1,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,
        41,42,43,44,45,46,47,48,49,50,51,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1
    };
    size_t outLen = 0;
    uint32_t buf  = 0;
    int bits      = 0;
    for (int i = 0; i < (int)input.length(); i++) {
        char c = input[i];
        if (c == '=') break;
        int8_t val = b64index[(uint8_t)c];
        if (val < 0) continue;
        buf  = (buf << 6) | val;
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            if (outLen < maxLen) output[outLen++] = (uint8_t)(buf >> bits);
        }
    }
    return (int)outLen;
}

// =========================================
// TÌM SLOT TRỐNG — AS1 và AS2 riêng biệt
// Prefix Preferences: "as1_" và "as2_"
// =========================================
int findFreeSlotAS1() {
    for (int slot = 1; slot <= 127; slot++) {
        if (!prefs.isKey(("as1_user_" + String(slot)).c_str())) return slot;
    }
    return -1;
}

int findFreeSlotAS2() {
    for (int slot = 1; slot <= 127; slot++) {
        if (!prefs.isKey(("as2_user_" + String(slot)).c_str())) return slot;
    }
    return -1;
}

// =========================================
// HELPER — Gửi lệnh UpChar thủ công cho AS608
// =========================================
static uint8_t sendUploadCharCommand(HardwareSerial& serial) {
    uint8_t cmd[] = {
        0xEF, 0x01, 0xFF, 0xFF, 0xFF, 0xFF,
        0x01, 0x00, 0x04, 0x09, 0x01,
        0x00, 0x0F
    };
    serial.write(cmd, sizeof(cmd));
    uint32_t t = millis();
    while (serial.available() < 12 && millis() - t < 1000);
    uint8_t resp[12] = {0};
    int n = 0;
    t = millis();
    while (n < 12 && millis() - t < 500) {
        if (serial.available()) resp[n++] = serial.read();
    }
    for (int i = 0; i < n - 1; i++) {
        if (resp[i] == 0xEF && resp[i+1] == 0x01) {
            return (resp[i + 9] == 0x00) ? FINGERPRINT_OK : resp[i + 9];
        }
    }
    return FINGERPRINT_PACKETRECIEVEERR;
}

// =========================================
// DOWNLOAD TEMPLATE — AS608 (512 bytes)
// =========================================
uint8_t manualDownloadModelAS(Adafruit_Fingerprint& finger, HardwareSerial& serial, uint8_t* dest) {
    uint8_t p = finger.getModel();
    if (p != FINGERPRINT_OK) return p;

    uint16_t bytesRead = 0;
    uint32_t startTimer = millis();
    while (bytesRead < 512) {
        if (millis() - startTimer > 3000) return FINGERPRINT_TIMEOUT;
        if (!serial.available()) continue;
        if (serial.read() != 0xEF) continue;
        uint32_t t = millis();
        while (!serial.available() && millis() - t < 200);
        if (serial.read() != 0x01) continue;
        for (int i = 0; i < 4; i++) {
            t = millis();
            while (!serial.available() && millis() - t < 200);
            serial.read();
        }
        t = millis();
        while (!serial.available() && millis() - t < 200);
        uint8_t pid = serial.read();
        t = millis();
        while (serial.available() < 2 && millis() - t < 200);
        uint16_t len = ((uint16_t)serial.read() << 8) | serial.read();
        uint16_t dataLen = len - 2;
        for (uint16_t i = 0; i < dataLen && bytesRead < 512; i++) {
            t = millis();
            while (!serial.available() && millis() - t < 200);
            dest[bytesRead++] = serial.read();
        }
        for (int i = 0; i < 2; i++) {
            t = millis();
            while (!serial.available() && millis() - t < 200);
            serial.read();
        }
        if (pid == 0x08) break;
    }
    return FINGERPRINT_OK;
}

// =========================================
// UPLOAD TEMPLATE VÀO AS608 (512 bytes)
// =========================================
uint8_t uploadFingerprintTemplate(Adafruit_Fingerprint& finger, HardwareSerial& serial,
                                   uint16_t slot, String base64Data) {
    uint8_t templateBuffer[512] = {0};
    int len = base64Decode(base64Data, templateBuffer, 512);
    if (len < 256) {
        Serial.println("[AS608] Template quá ngắn sau decode");
        return 0xFF;
    }

    if (sendUploadCharCommand(serial) != FINGERPRINT_OK) {
        Serial.println("[AS608] UploadChar command thất bại");
        return 0xFF;
    }

    const uint16_t PKT_DATA = 64;
    int totalPkts = (len + PKT_DATA - 1) / PKT_DATA;

    for (int i = 0; i < totalPkts; i++) {
        uint8_t pid   = (i == totalPkts - 1) ? 0x08 : 0x02;
        uint16_t dLen = (i == totalPkts - 1 && len % PKT_DATA != 0)
                        ? (len % PKT_DATA) : PKT_DATA;

        uint8_t pkt[80];
        int idx = 0;
        pkt[idx++] = 0xEF; pkt[idx++] = 0x01;
        pkt[idx++] = 0xFF; pkt[idx++] = 0xFF; pkt[idx++] = 0xFF; pkt[idx++] = 0xFF;
        pkt[idx++] = pid;
        uint16_t plen = dLen + 2;
        pkt[idx++] = plen >> 8;
        pkt[idx++] = plen & 0xFF;

        uint16_t sum = pid + (plen >> 8) + (plen & 0xFF);
        for (uint16_t j = 0; j < dLen; j++) {
            uint8_t b = templateBuffer[i * PKT_DATA + j];
            pkt[idx++] = b;
            sum += b;
        }
        pkt[idx++] = sum >> 8;
        pkt[idx++] = sum & 0xFF;

        serial.write(pkt, idx);
        delay(5);
    }

    uint8_t p = finger.storeModel(slot);
    if (p != FINGERPRINT_OK) {
        Serial.printf("[AS608] StoreModel thất bại: 0x%02X\n", p);
        return 0xFF;
    }
    Serial.printf("[AS608] Upload template OK → slot #%d\n", slot);
    return FINGERPRINT_OK;
}

// =========================================
// SYNC SAU ENROLL — chỉ ghi đúng sensor đang enroll
// sensorIdx: 0 = AS1, 1 = AS2
// =========================================
void syncAfterEnroll(uint8_t slot, String name, String code, uint8_t sensorIdx) {
    const String prefix     = (sensorIdx == 0) ? "as1_" : "as2_";
    const String sensorName = (sensorIdx == 0) ? "AS608_1" : "AS608_2";

    // Ghi Preferences chỉ cho sensor đang enroll
    prefs.putString((prefix + "user_" + String(slot)).c_str(), name);
    prefs.putString((prefix + "code_" + String(slot)).c_str(), code);
    prefs.putInt((prefix + "slot_" + code).c_str(), slot);

    Serial.printf("[Enroll][%s] Prefs OK: slot=%d code=%s name=%s\n",
                  sensorName.c_str(), slot, code.c_str(), name.c_str());

    if (WiFi.status() != WL_CONNECTED) return;

    // Download template từ đúng sensor vừa enroll
    uint8_t templateBuffer[512];
    Adafruit_Fingerprint& finger = (sensorIdx == 0) ? fingerAS1 : fingerAS2;
    HardwareSerial&       serial = (sensorIdx == 0) ? fpSerialAS1 : fpSerialAS2;

    JsonDocument doc;
    doc["slot"]   = slot;
    doc["name"]   = name;
    doc["code"]   = code;
    doc["sensor"] = sensorName;  // "AS608_1" hoặc "AS608_2"

    if (manualDownloadModelAS(finger, serial, templateBuffer) == FINGERPRINT_OK) {
        doc["fingerprint"] = base64Encode(templateBuffer, 512);
        Serial.printf("[Enroll][%s] Download template OK → đẩy server\n", sensorName.c_str());
    } else {
        Serial.printf("[Enroll][%s] Download template thất bại — vẫn sync metadata\n", sensorName.c_str());
    }

    HTTPClient http;
    http.begin(getServerBase() + "/api/sync-user");
    http.addHeader("Content-Type", "application/json");
    String payload;
    serializeJson(doc, payload);
    int httpCode = http.POST(payload);
    Serial.printf("[Enroll][%s] syncAfterEnroll: HTTP %d\n", sensorName.c_str(), httpCode);
    http.end();
}

// =========================================
// ENROLL AS1 — chỉ enroll trên AS1
// =========================================
uint8_t enrollAS1(uint8_t slot, String name, String code) {
    Serial.println("[AS1] Đặt ngón tay lên cảm biến 1...");
    pixels.setPixelColor(0, pixels.Color(255, 165, 0));
    pixels.show();

    uint32_t t = millis();
    while (fingerAS1.getImage() != FINGERPRINT_OK) {
        server.handleClient();
        if (millis() - t > 15000) { pixels.clear(); pixels.show(); return FINGERPRINT_TIMEOUT; }
    }
    if (fingerAS1.image2Tz(1) != FINGERPRINT_OK) {
        pixels.clear(); pixels.show(); return FINGERPRINT_IMAGEMESS;
    }

    Serial.println("[AS1] Nhấc tay ra...");
    delay(1000);
    t = millis();
    while (fingerAS1.getImage() != FINGERPRINT_NOFINGER) {
        server.handleClient();
        if (millis() - t > 5000) break;
    }
    delay(500);

    Serial.println("[AS1] Đặt lại ngón tay...");
    t = millis();
    while (fingerAS1.getImage() != FINGERPRINT_OK) {
        server.handleClient();
        if (millis() - t > 15000) { pixels.clear(); pixels.show(); return FINGERPRINT_TIMEOUT; }
    }
    if (fingerAS1.image2Tz(2)      != FINGERPRINT_OK) { pixels.clear(); pixels.show(); return FINGERPRINT_IMAGEMESS; }
    if (fingerAS1.createModel()    != FINGERPRINT_OK) { pixels.clear(); pixels.show(); return FINGERPRINT_ENROLLMISMATCH; }
    if (fingerAS1.storeModel(slot) != FINGERPRINT_OK) { pixels.clear(); pixels.show(); return FINGERPRINT_BADLOCATION; }

    Serial.printf("[AS1]  Enroll slot #%d OK\n", slot);
    syncAfterEnroll(slot, name, code, 0);  // sensorIdx=0 → chỉ ghi as1_
    pixels.clear(); pixels.show();
    return slot;
}

// =========================================
// ENROLL AS2 — chỉ enroll trên AS2
// =========================================
uint8_t enrollAS2(uint8_t slot, String name, String code) {
    Serial.println("[AS2] Đặt ngón tay lên cảm biến 2...");
    pixels.setPixelColor(0, pixels.Color(0, 165, 255));
    pixels.show();

    uint32_t t = millis();
    while (fingerAS2.getImage() != FINGERPRINT_OK) {
        server.handleClient();
        if (millis() - t > 15000) { pixels.clear(); pixels.show(); return FINGERPRINT_TIMEOUT; }
    }
    if (fingerAS2.image2Tz(1) != FINGERPRINT_OK) {
        pixels.clear(); pixels.show(); return FINGERPRINT_IMAGEMESS;
    }

    Serial.println("[AS2] Nhấc tay ra...");
    delay(1000);
    t = millis();
    while (fingerAS2.getImage() != FINGERPRINT_NOFINGER) {
        server.handleClient();
        if (millis() - t > 5000) break;
    }
    delay(500);

    Serial.println("[AS2] Đặt lại ngón tay...");
    t = millis();
    while (fingerAS2.getImage() != FINGERPRINT_OK) {
        server.handleClient();
        if (millis() - t > 15000) { pixels.clear(); pixels.show(); return FINGERPRINT_TIMEOUT; }
    }
    if (fingerAS2.image2Tz(2)      != FINGERPRINT_OK) { pixels.clear(); pixels.show(); return FINGERPRINT_IMAGEMESS; }
    if (fingerAS2.createModel()    != FINGERPRINT_OK) { pixels.clear(); pixels.show(); return FINGERPRINT_ENROLLMISMATCH; }
    if (fingerAS2.storeModel(slot) != FINGERPRINT_OK) { pixels.clear(); pixels.show(); return FINGERPRINT_BADLOCATION; }

    Serial.printf("[AS2]  Enroll slot #%d OK\n", slot);
    syncAfterEnroll(slot, name, code, 1);  // sensorIdx=1 → chỉ ghi as2_
    pixels.clear(); pixels.show();
    return slot;
}

// =========================================
// ENROLL FINGER — entry point theo sensorIdx
// sensorIdx: 0 = AS1, 1 = AS2
// =========================================
uint8_t enrollFinger(String name, String code, uint8_t sensorIdx) {
    pixels.setPixelColor(0, pixels.Color(255, 255, 0));
    pixels.show();

    if (sensorIdx == 0) {
        // Kiểm tra code đã tồn tại trên AS1 chưa
        if (prefs.getInt(("as1_slot_" + code).c_str(), -1) != -1) {
            Serial.printf("[AS1] Code=%s đã tồn tại trên AS1\n", code.c_str());
            pixels.clear(); pixels.show();
            return FINGERPRINT_BADLOCATION;
        }
        int slot = findFreeSlotAS1();
        if (slot == -1) {
            Serial.println("[AS1] AS1 đầy!");
            pixels.clear(); pixels.show();
            return FINGERPRINT_BADLOCATION;
        }
        fingerAS1.deleteModel(slot);  // Dọn slot cũ nếu có dữ liệu rác
        return enrollAS1((uint8_t)slot, name, code);

    } else {
        // Kiểm tra code đã tồn tại trên AS2 chưa
        if (prefs.getInt(("as2_slot_" + code).c_str(), -1) != -1) {
            Serial.printf("[AS2] Code=%s đã tồn tại trên AS2\n", code.c_str());
            pixels.clear(); pixels.show();
            return FINGERPRINT_BADLOCATION;
        }
        int slot = findFreeSlotAS2();
        if (slot == -1) {
            Serial.println("[AS2] AS2 đầy!");
            pixels.clear(); pixels.show();
            return FINGERPRINT_BADLOCATION;
        }
        fingerAS2.deleteModel(slot);  // Dọn slot cũ nếu có dữ liệu rác
        return enrollAS2((uint8_t)slot, name, code);
    }
}

// =========================================
// CHECK FINGERPRINT — AS1
// =========================================
void checkAS1() {
    uint8_t p = fingerAS1.getImage();
    if (p != FINGERPRINT_OK) return;

    p = fingerAS1.image2Tz();
    if (p != FINGERPRINT_OK) return;

    p = fingerAS1.fingerFastSearch();
    if (p == FINGERPRINT_OK) {
        if (fingerAS1.confidence > 100) {
            uint16_t id    = fingerAS1.fingerID;
            uint16_t score = fingerAS1.confidence;
            String name = prefs.getString(("as1_user_" + String(id)).c_str(), "Unknown");
            String code = prefs.getString(("as1_code_" + String(id)).c_str(), "");
            Serial.printf("[AS1]  Match! Slot:%d Code:%s Name:%s Score:%d\n",
                          id, code.c_str(), name.c_str(), score);
            logAccessAS1(id, "", name, true, code);
            openLock();
        } else {
            Serial.printf("[AS1] Tin cậy quá thấp (%d) → từ chối\n", fingerAS1.confidence);
            logAccessAS1(0, "", "Unknown", false, "");
            denyAccess();
        }
    } else if (p == FINGERPRINT_NOTFOUND) {
        Serial.println("[AS1] Không khớp");
        logAccessAS1(0, "", "Unknown", false, "");
        denyAccess();
    }
}

// =========================================
// CHECK FINGERPRINT — AS2
// =========================================
void checkAS2() {
    uint8_t p = fingerAS2.getImage();
    if (p != FINGERPRINT_OK) return;

    p = fingerAS2.image2Tz();
    if (p != FINGERPRINT_OK) return;

    p = fingerAS2.fingerFastSearch();
    if (p == FINGERPRINT_OK) {
        if (fingerAS2.confidence > 100) {
            uint16_t id    = fingerAS2.fingerID;
            uint16_t score = fingerAS2.confidence;
            String name = prefs.getString(("as2_user_" + String(id)).c_str(), "Unknown");
            String code = prefs.getString(("as2_code_" + String(id)).c_str(), "");
            Serial.printf("[AS2]  Match! Slot:%d Code:%s Name:%s Score:%d\n",
                          id, code.c_str(), name.c_str(), score);
            logAccessAS2(id, "", name, true, code);
            openLock();
        } else {
            Serial.printf("[AS2] Tin cậy quá thấp (%d) → từ chối\n", fingerAS2.confidence);
            logAccessAS2(0, "", "Unknown", false, "");
            denyAccess();
        }
    } else if (p == FINGERPRINT_NOTFOUND) {
        Serial.println("[AS2] Không khớp");
        logAccessAS2(0, "", "Unknown", false, "");
        denyAccess();
    }
}

// =========================================
// CHECK FINGERPRINT — GỌI CẢ 2
// =========================================
void checkFingerprint() {
    checkAS1();
    checkAS2();
}

// =========================================
// SYNC TOÀN BỘ TỪ SERVER
// sensorIdx: 0 = AS1, 1 = AS2
// server trả về fingerprint đúng cột theo sensor
// =========================================
void syncFromServer(uint8_t sensorIdx) {
    if (WiFi.status() != WL_CONNECTED) { Serial.println("syncFromServer: không có WiFi"); return; }

    const String prefix     = (sensorIdx == 0) ? "as1_" : "as2_";
    const String sensorName = (sensorIdx == 0) ? "AS608_1" : "AS608_2";
    Adafruit_Fingerprint& finger = (sensorIdx == 0) ? fingerAS1 : fingerAS2;
    HardwareSerial&       serial = (sensorIdx == 0) ? fpSerialAS1 : fpSerialAS2;

    Serial.printf("=== BẮT ĐẦU SYNC TỪ SERVER [%s] ===\n", sensorName.c_str());
    pixels.setPixelColor(0, pixels.Color(128, 0, 128));
    pixels.show();

    HTTPClient http;
    http.begin(getServerBase() + "/api/users?sensor=" + sensorName);
    int httpCode = http.GET();
    if (httpCode != 200) {
        Serial.printf("syncFromServer[%s]: thất bại HTTP %d\n", sensorName.c_str(), httpCode);
        http.end(); pixels.clear(); pixels.show();
        return;
    }

    String body = http.getString();
    http.end();

    JsonDocument doc;
    if (deserializeJson(doc, body)) {
        Serial.println("syncFromServer: parse JSON thất bại");
        pixels.clear(); pixels.show();
        return;
    }

    int successCount = 0;
    for (JsonObject user : doc.as<JsonArray>()) {
        String ucode   = user["code"].as<String>();
        String uname   = user["name"].as<String>();
        String dataB64 = user["fingerprint"].as<String>();

        if (dataB64.length() < 100) {
            Serial.printf("[Sync][%s] code=%s: không có template\n", sensorName.c_str(), ucode.c_str());
            continue;
        }

        // Tìm slot đã cấp hoặc cấp slot mới — riêng cho từng sensor
        int slot = prefs.getInt((prefix + "slot_" + ucode).c_str(), -1);
        if (slot == -1) {
            slot = (sensorIdx == 0) ? findFreeSlotAS1() : findFreeSlotAS2();
        }
        if (slot == -1) {
            Serial.printf("[Sync][%s] Đầy — dừng sync\n", sensorName.c_str());
            break;
        }

        Serial.printf("[Sync][%s] code=%s → slot #%d\n", sensorName.c_str(), ucode.c_str(), slot);
        if (uploadFingerprintTemplate(finger, serial, (uint16_t)slot, dataB64) == FINGERPRINT_OK) {
            prefs.putString((prefix + "user_" + String(slot)).c_str(), uname);
            prefs.putString((prefix + "code_" + String(slot)).c_str(), ucode);
            prefs.putInt((prefix + "slot_" + ucode).c_str(), slot);
            successCount++;
        }
        delay(100);
    }

    Serial.printf("=== SYNC [%s] XONG: %d user ===\n", sensorName.c_str(), successCount);
    pixels.clear(); pixels.show();
}

// =========================================
// SYNC 1 USER TỪ SERVER
// sensorIdx: 0 = AS1, 1 = AS2
// =========================================
void syncUserFromServer(String code, uint8_t sensorIdx) {
    if (WiFi.status() != WL_CONNECTED) { Serial.println("syncUser: không có WiFi"); return; }

    const String prefix     = (sensorIdx == 0) ? "as1_" : "as2_";
    const String sensorName = (sensorIdx == 0) ? "AS608_1" : "AS608_2";
    Adafruit_Fingerprint& finger = (sensorIdx == 0) ? fingerAS1 : fingerAS2;
    HardwareSerial&       serial = (sensorIdx == 0) ? fpSerialAS1 : fpSerialAS2;

    Serial.printf("=== SYNC USER code=%s [%s] ===\n", code.c_str(), sensorName.c_str());

    HTTPClient http;
    http.begin(getServerBase() + "/api/users?sensor=" + sensorName);
    int httpCode = http.GET();
    if (httpCode != 200) { http.end(); return; }

    String body = http.getString();
    http.end();

    JsonDocument doc;
    if (deserializeJson(doc, body)) return;

    for (JsonObject user : doc.as<JsonArray>()) {
        String ucode = user["code"].as<String>();
        if (ucode != code) continue;

        String dataB64 = user["fingerprint"].as<String>();
        String uname   = user["name"].as<String>();

        if (dataB64.length() < 100) {
            Serial.printf("[Sync][%s] code=%s không có template\n", sensorName.c_str(), code.c_str());
            return;
        }

        int slot = prefs.getInt((prefix + "slot_" + code).c_str(), -1);
        if (slot == -1) slot = (sensorIdx == 0) ? findFreeSlotAS1() : findFreeSlotAS2();
        if (slot == -1) {
            Serial.printf("[Sync][%s] Đầy\n", sensorName.c_str());
            return;
        }

        if (uploadFingerprintTemplate(finger, serial, (uint16_t)slot, dataB64) == FINGERPRINT_OK) {
            prefs.putString((prefix + "user_" + String(slot)).c_str(), uname);
            prefs.putString((prefix + "code_" + String(slot)).c_str(), ucode);
            prefs.putInt((prefix + "slot_" + ucode).c_str(), slot);
            Serial.printf("[Sync][%s] slot #%d OK\n", sensorName.c_str(), slot);
        }
        return;
    }
    Serial.printf("syncUser: không tìm thấy code=%s trên server\n", code.c_str());
}

// =========================================
// XÓA TOÀN BỘ
// =========================================
void emptyDatabaseAS1() {
    Serial.println("[AS1] Xóa database...");
    if (fingerAS1.emptyDatabase() != FINGERPRINT_OK) { Serial.println("[AS1] Lỗi xóa sensor"); return; }
    for (int slot = 1; slot <= 127; slot++) {
        String nk = "as1_user_" + String(slot);
        String ck = "as1_code_" + String(slot);
        if (prefs.isKey(nk.c_str())) {
            String c = prefs.getString(ck.c_str(), "");
            prefs.remove(nk.c_str()); prefs.remove(ck.c_str());
            if (c.length()) prefs.remove(("as1_slot_" + c).c_str());
        }
    }
    Serial.println("[AS1] ✅ Xóa xong");
}

void emptyDatabaseAS2() {
    Serial.println("[AS2] Xóa database...");
    if (fingerAS2.emptyDatabase() != FINGERPRINT_OK) { Serial.println("[AS2] Lỗi xóa sensor"); return; }
    for (int slot = 1; slot <= 127; slot++) {
        String nk = "as2_user_" + String(slot);
        String ck = "as2_code_" + String(slot);
        if (prefs.isKey(nk.c_str())) {
            String c = prefs.getString(ck.c_str(), "");
            prefs.remove(nk.c_str()); prefs.remove(ck.c_str());
            if (c.length()) prefs.remove(("as2_slot_" + c).c_str());
        }
    }
    Serial.println("[AS2] ✅ Xóa xong");
}

// =========================================
// XÓA 1 TEMPLATE
// =========================================
bool deleteTemplateAS1(uint16_t slot) {
    return fingerAS1.deleteModel(slot) == FINGERPRINT_OK;
}

bool deleteTemplateAS2(uint16_t slot) {
    return fingerAS2.deleteModel(slot) == FINGERPRINT_OK;
}