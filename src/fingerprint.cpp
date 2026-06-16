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
// TÌM SLOT TRỐNG
// =========================================
int findFreeSlotAS() {
    for (int slot = 1; slot <= 127; slot++) {
        if (!prefs.isKey(("as_user_" + String(slot)).c_str())) return slot;
    }
    return -1;
}

int findFreeSlotRS() {
    for (int slot = 1; slot <= 199; slot++) {  // R503 đánh số từ 0, tối đa 200 slot
        if (!prefs.isKey(("rs_user_" + String(slot)).c_str())) return slot;
    }
    return -1;
}

// =========================================
// HELPER — AS608: gửi lệnh UpChar thủ công
// =========================================
static uint8_t as608_sendUploadCharCommand() {
    uint8_t cmd[] = {
        0xEF, 0x01, 0xFF, 0xFF, 0xFF, 0xFF,
        0x01, 0x00, 0x04, 0x09, 0x01,
        0x00, 0x0F
    };
    fpSerialAS.write(cmd, sizeof(cmd));
    uint32_t t = millis();
    while (fpSerialAS.available() < 12 && millis() - t < 1000);
    uint8_t resp[12] = {0};
    int n = 0;
    t = millis();
    while (n < 12 && millis() - t < 500) {
        if (fpSerialAS.available()) resp[n++] = fpSerialAS.read();
    }
    for (int i = 0; i < n - 1; i++) {
        if (resp[i] == 0xEF && resp[i+1] == 0x01) {
            uint8_t cc = resp[i + 9];
            return (cc == 0x00) ? FINGERPRINT_OK : cc;
        }
    }
    return FINGERPRINT_PACKETRECIEVEERR;
}

// =========================================
// SYNC SAU ENROLL — AS608
// =========================================
void syncAfterEnrollAS(uint8_t slot, String name, String code) {
    prefs.putString(("as_user_" + String(slot)).c_str(), name);
    prefs.putString(("as_code_" + String(slot)).c_str(), code);
    prefs.putInt(("as_slot_" + code).c_str(), slot);

    Serial.printf("[AS608] Đã lưu flash: slot=%d code=%s name=%s\n", slot, code.c_str(), name.c_str());

    if (WiFi.status() != WL_CONNECTED) return;

    uint8_t templateBuffer[512];
    HTTPClient http;
    http.begin(getServerBase() + "/api/sync-user");
    http.addHeader("Content-Type", "application/json");
    JsonDocument doc;
    doc["slot"]   = slot;
    doc["name"]   = name;
    doc["code"]   = code;
    doc["sensor"] = "AS608";

    if (manualDownloadModelAS(templateBuffer) == FINGERPRINT_OK) {
        doc["fingerprint"] = base64Encode(templateBuffer, 512);
        Serial.println("[AS608] Download template OK, đẩy lên server");
    } else {
        Serial.println("[AS608] Download template thất bại — sync không có template");
    }

    String payload;
    serializeJson(doc, payload);
    int httpCode = http.POST(payload);
    Serial.printf("[AS608] syncAfterEnroll: HTTP %d\n", httpCode);
    http.end();
}

// =========================================
// SYNC SAU ENROLL — R503
// =========================================
void syncAfterEnrollRS(uint8_t slot, String name, String code) {
    prefs.putString(("rs_user_" + String(slot)).c_str(), name);
    prefs.putString(("rs_code_" + String(slot)).c_str(), code);
    prefs.putInt(("rs_slot_" + code).c_str(), slot);

    Serial.printf("[R503] ✓ Enroll OK - slot=%d | code=%s\n", slot, code.c_str());

    if (WiFi.status() != WL_CONNECTED) return;

    uint8_t templateBuffer[1024] = {0};  // Giảm buffer
    uint16_t len = 0;

    HTTPClient http;
    http.begin(getServerBase() + "/api/sync-user");
    http.addHeader("Content-Type", "application/json");
    JsonDocument doc;
    doc["slot"] = slot;
    doc["name"] = name;
    doc["code"] = code;
    doc["sensor"] = "R503";

    if (r503.downloadTemplate(slot, templateBuffer, &len)) {
        if (len > 768) len = 768;  // Cắt bớt nếu dư
        doc["fingerprint"] = base64Encode(templateBuffer, len);
        Serial.printf("[R503] Download template OK (%d bytes) → đẩy lên server\n", len);
    } else {
        Serial.println("[R503] Download template thất bại");
    }

    String payload;
    serializeJson(doc, payload);
    int httpCode = http.POST(payload);
    Serial.printf("[R503] syncAfterEnroll: HTTP %d\n", httpCode);
    http.end();
}

// =========================================
// ENROLL AS608
// =========================================
uint8_t enrollAS608(uint8_t slot, String name, String code) {
    Serial.println("[AS608] Đặt ngón tay lên cảm biến...");

    uint32_t t = millis();
    while (fingerAS.getImage() != FINGERPRINT_OK) {
        server.handleClient();
        if (millis() - t > 15000) { pixels.clear(); pixels.show(); return FINGERPRINT_TIMEOUT; }
    }
    if (fingerAS.image2Tz(1) != FINGERPRINT_OK) { pixels.clear(); pixels.show(); return FINGERPRINT_IMAGEMESS; }

    Serial.println("[AS608] Nhấc tay ra...");
    delay(1000);
    t = millis();
    while (fingerAS.getImage() != FINGERPRINT_NOFINGER) {
        server.handleClient();
        if (millis() - t > 5000) break;
    }
    delay(500);

    Serial.println("[AS608] Đặt lại ngón tay...");
    t = millis();
    while (fingerAS.getImage() != FINGERPRINT_OK) {
        server.handleClient();
        if (millis() - t > 15000) { pixels.clear(); pixels.show(); return FINGERPRINT_TIMEOUT; }
    }
    if (fingerAS.image2Tz(2)     != FINGERPRINT_OK) { pixels.clear(); pixels.show(); return FINGERPRINT_IMAGEMESS; }
    if (fingerAS.createModel()   != FINGERPRINT_OK) { pixels.clear(); pixels.show(); return FINGERPRINT_ENROLLMISMATCH; }
    if (fingerAS.storeModel(slot) != FINGERPRINT_OK) { pixels.clear(); pixels.show(); return FINGERPRINT_BADLOCATION; }

    syncAfterEnrollAS(slot, name, code);
    pixels.clear(); pixels.show();
    return slot;
}

// =========================================
// ENROLL R503 — DÙNG DRIVER
// =========================================
uint8_t enrollR503(uint8_t slot, String name, String code) {
    pixels.setPixelColor(0, pixels.Color(255, 165, 0));
    pixels.show();

    Serial.printf("[R503] Enrolling slot #%d | Code: %s\n", slot, code.c_str());
    
    if (!r503.enroll(slot)) {
        Serial.println("[R503] Enroll thất bại");
        pixels.clear(); pixels.show();
        return FINGERPRINT_ENROLLMISMATCH;
    }

    syncAfterEnrollRS(slot, name, code);
    pixels.clear(); pixels.show();
    return slot;
}

// =========================================
// ENROLL CHUNG
// sensorType: 0 = AS608, 1 = R503
// =========================================
uint8_t enrollFinger(String name, String code, uint8_t sensorType) {
    pixels.setPixelColor(0, pixels.Color(255, 255, 0));
    pixels.show();

    if (sensorType == 0) { // AS608
        int slot = findFreeSlotAS();
        if (slot == -1) return FINGERPRINT_BADLOCATION;
        fingerAS.deleteModel(slot);
        return enrollAS608((uint8_t)slot, name, code);
    } else { // R503
        int slot = findFreeSlotRS();
        if (slot == -1) return FINGERPRINT_BADLOCATION;
        // KHÔNG dùng fingerRS.deleteModel nữa
        return enrollR503((uint8_t)slot, name, code);
    }
}

// =========================================
// DOWNLOAD TEMPLATE — AS608 (512 bytes)
// =========================================
uint8_t manualDownloadModelAS(uint8_t* dest) {
    uint8_t p = fingerAS.getModel();
    if (p != FINGERPRINT_OK) return p;

    uint16_t bytesRead = 0;
    uint32_t startTimer = millis();
    while (bytesRead < 512) {
        if (millis() - startTimer > 3000) {
            Serial.println("[AS608] manualDownloadModel: Timeout!");
            return FINGERPRINT_TIMEOUT;
        }
        if (!fpSerialAS.available()) continue;
        if (fpSerialAS.read() != 0xEF) continue;
        uint32_t t = millis();
        while (!fpSerialAS.available() && millis() - t < 200);
        if (fpSerialAS.read() != 0x01) continue;
        for (int i = 0; i < 4; i++) {
            t = millis();
            while (!fpSerialAS.available() && millis() - t < 200);
            fpSerialAS.read();
        }
        t = millis();
        while (!fpSerialAS.available() && millis() - t < 200);
        uint8_t pid = fpSerialAS.read();
        t = millis();
        while (fpSerialAS.available() < 2 && millis() - t < 200);
        uint16_t len = ((uint16_t)fpSerialAS.read() << 8) | fpSerialAS.read();
        uint16_t dataLen = len - 2;
        for (uint16_t i = 0; i < dataLen && bytesRead < 512; i++) {
            t = millis();
            while (!fpSerialAS.available() && millis() - t < 200);
            dest[bytesRead++] = fpSerialAS.read();
        }
        for (int i = 0; i < 2; i++) {
            t = millis();
            while (!fpSerialAS.available() && millis() - t < 200);
            fpSerialAS.read();
        }
        if (pid == 0x08) break;
    }
    return FINGERPRINT_OK;
}

// =========================================
// DOWNLOAD TEMPLATE — R503
// =========================================
uint8_t manualDownloadModelRS(uint8_t* dest) {
    uint16_t len = 0;
    if (r503.downloadTemplate(0, dest, &len)) {  // ID tạm thời
        return FINGERPRINT_OK;
    }
    return FINGERPRINT_PACKETRECIEVEERR;
}

// =========================================
// UPLOAD TEMPLATE VÀO AS608 (512 bytes)
// =========================================
uint8_t uploadFingerprintTemplateAS(uint16_t slot, String base64Data) {
    uint8_t templateBuffer[512] = {0};
    int len = base64Decode(base64Data, templateBuffer, 512);
    if (len < 256) {
        Serial.println("[AS608] Template quá ngắn sau decode");
        return 0xFF;
    }

    if (as608_sendUploadCharCommand() != FINGERPRINT_OK) {
        Serial.println("[AS608] UploadChar command thất bại");
        return 0xFF;
    }

    const uint16_t PKT_DATA = 64;
    int totalPkts = (len + PKT_DATA - 1) / PKT_DATA;

    for (int i = 0; i < totalPkts; i++) {
        uint8_t pid = (i == totalPkts - 1) ? 0x08 : 0x02;
        uint16_t dLen = (i == totalPkts - 1 && len % PKT_DATA != 0) ? (len % PKT_DATA) : PKT_DATA;

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

        fpSerialAS.write(pkt, idx);
        delay(5);
    }

    uint8_t p = fingerAS.storeModel(slot);
    if (p != FINGERPRINT_OK) {
        Serial.printf("[AS608] StoreModel thất bại: 0x%02X\n", p);
        return 0xFF;
    }

    Serial.printf("[AS608] Upload template OK → slot #%d\n", slot);
    return FINGERPRINT_OK;
}

// =========================================
// UPLOAD TEMPLATE R503 — FINAL
// =========================================
uint8_t uploadFingerprintTemplateRS(uint16_t slot, String base64Data) {
    uint8_t templateBuffer[1024] = {0};
    
    int len = base64Decode(base64Data, templateBuffer, 1024);
    Serial.printf("[R503] Decoded: %d bytes\n", len);

    if (len != 768) {
        Serial.println("[R503] Template sai kích thước!");
        return 0xFF;
    }

    Serial.printf("[R503] Đang tiến hành nạp vân tay vào slot chỉ định = %d\n", slot);

    // Thử upload nhiều lần 
    for (int i = 1; i <= 3; i++) {
        Serial.printf("[R503] Attempt %d...\n", i);
        delay(500);
        
        // ✅ FIX: Lưu trực tiếp vào biến `slot` nhận từ hàm gọi Sync
        if (r503.uploadTemplate(slot, templateBuffer, 768)) {
            Serial.printf("[R503] ✅ THÀNH CÔNG slot #%d\n", slot);
            return FINGERPRINT_OK;
        }
    }

    Serial.printf("[R503] ❌ Không thể upload template vào slot %d\n", slot);
    return 0xFF;
}

// =========================================
// CHECK FINGERPRINT — AS608
// =========================================
void checkAS608() {
    // Giả sử hàm kiểm tra nhanh hoặc đọc ảnh của AS608 nằm ở đây
    uint8_t p = fingerAS.getImage();
    if (p != FINGERPRINT_OK) return;

    p = fingerAS.image2Tz();
    if (p != FINGERPRINT_OK) return;

    // Thực hiện tìm kiếm nhanh trong thư viện
    p = fingerAS.fingerFastSearch();
    
    if (p == FINGERPRINT_OK) {
        //KIỂM TRA NGƯỠNG ĐỘ TIN CẬY (CONFIDENCE > 100)
        if (fingerAS.confidence > 100) {
            uint16_t matchedID = fingerAS.fingerID;
            uint16_t score = fingerAS.confidence;

            // Lấy thông tin User từ Preferences nâng cấp bảo mật
            String name = prefs.getString(("as_user_" + String(matchedID)).c_str(), "Unknown");
            String code = prefs.getString(("as_code_" + String(matchedID)).c_str(), "");

            Serial.printf("[AS608] ✅ Match! Slot: %d | Code: %s | Name: %s | Confidence: %d\n",
                          matchedID, code.c_str(), name.c_str(), score);

            logAccessAS(matchedID, "", name, true, code);
            openLock(); // Kích hoạt mở khóa
        } else {
            // Trường hợp vân tay có khớp nhưng điểm số quá thấp (chạm không đều, mờ...)
            Serial.printf("[AS608] ⚠️ Khớp vân tay nhưng độ tin cậy quá thấp (Confidence: %d <= 100) -> TỪ CHỐI\n", fingerAS.confidence);
            logAccessAS(0, "", "Unknown", false, "");
            denyAccess(); // Báo lỗi, không mở cửa
        }
    } else if (p == FINGERPRINT_NOTFOUND) {
        Serial.println("[AS608] ❌ Vân tay hoàn toàn không khớp");
        logAccessAS(0, "", "Unknown", false, "");
        denyAccess();
    }
}

// =========================================
// CHECK FINGERPRINT — R503
// =========================================
void checkR503() {
    // Bước 1: Poll nhanh — có ngón tay không? (300ms, không blocking)
    if (!r503.isTouched()) return;

    r503.setLED(0x02, 0x03);

    // Bước 2: Có ngón tay → chạy nhận diện
    uint16_t matchedID = 0;
    uint16_t score = 0;

    if (r503.verify(&matchedID, &score)) {
        // KIỂM TRA NGƯỠNG ĐIỂM SỐ (SCORE THRESHOLD > 100)
        if (score > 100) {
            String name = prefs.getString(("rs_user_" + String(matchedID)).c_str(), "Unknown");
            String code = prefs.getString(("rs_code_" + String(matchedID)).c_str(), "");

            Serial.printf("[R503] ✅ Match! Slot: %d | Code: %s | Name: %s | Score: %d\n",
                          matchedID, code.c_str(), name.c_str(), score);
            
            r503.setLED(0x03, 0x04);
            logAccessRS(matchedID, "", name, true, code);
            openLock(); // Mở cửa
            r503.setLED(0x01, 0x07);
        } else {
            // Khớp vân tay nhưng điểm số quá thấp, từ chối mở cửa
            Serial.printf("[R503] ⚠️ Khớp vân tay nhưng điểm quá thấp (Score: %d <= 100) -> TỪ CHỐI\n", score);
            r503.setLED(0x02, 0x01);
            logAccessRS(0, "", "Unknown", false, "");
            denyAccess(); // Không mở cửa, báo đèn đỏ
            r503.setLED(0x01, 0x07);
        }
    } else {
        if (r503.getLastError() == 0x09) {
            Serial.println("[R503] ❌ Vân tay hoàn toàn không khớp");
            r503.setLED(0x02, 0x01);
            logAccessRS(0, "", "Unknown", false, "");
            denyAccess(); // Không mở cửa, báo đèn đỏ
            r503.setLED(0x01, 0x07);
        }
    }
}

// =========================================
// CHECK FINGERPRINT — GỌI CẢ 2
// =========================================
void checkFingerprint() {
    checkAS608();
    checkR503();
}

// =========================================
// SYNC TOÀN BỘ TỪ SERVER
// sensorType: 0 = AS608, 1 = R503
// =========================================
void syncFromServer(uint8_t sensorType) {
    if (WiFi.status() != WL_CONNECTED) { Serial.println("syncFromServer: không có WiFi"); return; }

    const char* label = (sensorType == 0) ? "AS608" : "R503";
    Serial.printf("=== BẮT ĐẦU SYNC TỪ SERVER [%s] ===\n", label);
    pixels.setPixelColor(0, pixels.Color(128, 0, 128));
    pixels.show();

    HTTPClient http;
    http.begin(getServerBase() + "/api/users?sensor=" + String(label));
    int httpCode = http.GET();
    if (httpCode != 200) {
        Serial.printf("syncFromServer [%s]: thất bại: %d\n", label, httpCode);
        http.end(); pixels.clear(); pixels.show();
        return;
    }

    String body = http.getString();
    http.end();

    JsonDocument doc;
    if (deserializeJson(doc, body)) {
        Serial.printf("syncFromServer [%s]: parse JSON thất bại\n", label);
        pixels.clear(); pixels.show();
        return;
    }

    JsonArray users = doc.as<JsonArray>();
    int successCount = 0;

    for (JsonObject user : users) {
        String ucode  = user["code"].as<String>();
        String uname  = user["name"].as<String>();
        String dataB64 = user["fingerprint"].as<String>();

        if (dataB64.length() < 100) {
            Serial.printf("[%s] code=%s: không có template\n", label, ucode.c_str());
            continue;
        }

        if (sensorType == 0) {
            int slot = prefs.getInt(("as_slot_" + ucode).c_str(), -1);
            if (slot == -1) slot = findFreeSlotAS();
            if (slot == -1) break;

            Serial.printf("[AS608] Nạp template code=%s → slot #%d...\n", ucode.c_str(), slot);
            uint8_t result = uploadFingerprintTemplateAS((uint16_t)slot, dataB64);
            if (result == FINGERPRINT_OK) {
                prefs.putString(("as_user_" + String(slot)).c_str(), uname);
                prefs.putString(("as_code_" + String(slot)).c_str(), ucode);
                prefs.putInt(("as_slot_" + ucode).c_str(), slot);
                successCount++;
            }
        } else {
            int slot = prefs.getInt(("rs_slot_" + ucode).c_str(), -1);
            if (slot == -1) slot = findFreeSlotRS();
            if (slot == -1) break;

            Serial.printf("[R503] Nạp template code=%s → slot #%d...\n", ucode.c_str(), slot);
            uint8_t result = uploadFingerprintTemplateRS((uint16_t)slot, dataB64);
            if (result == FINGERPRINT_OK) {
                prefs.putString(("rs_user_" + String(slot)).c_str(), uname);
                prefs.putString(("rs_code_" + String(slot)).c_str(), ucode);
                prefs.putInt(("rs_slot_" + ucode).c_str(), slot);
                successCount++;
            } else {
                Serial.printf("[R503] Nạp template thất bại slot=%d (code=%s)\n", slot, ucode.c_str());
            }
        }
        delay(100);
    }

    Serial.printf("=== SYNC XONG [%s]: %d user ===\n", label, successCount);
    pixels.clear(); pixels.show();
}

// =========================================
// SYNC 1 USER TỪ SERVER
// =========================================
void syncUserFromServer(String code, uint8_t sensorType) {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("syncUser: không có WiFi");
        return;
    }

    const char* label = (sensorType == 0) ? "AS608" : "R503";
    Serial.printf("=== SYNC USER code=%s [%s] ===\n", code.c_str(), label);

    HTTPClient http;
    http.begin(getServerBase() + "/api/users?sensor=" + String(label));
    int httpCode = http.GET();
    if (httpCode != 200) {
        Serial.printf("syncUser [%s]: thất bại: %d\n", label, httpCode);
        http.end();
        return;
    }

    String body = http.getString();
    http.end();

    JsonDocument doc;
    if (deserializeJson(doc, body)) {
        Serial.printf("syncUser [%s]: parse JSON thất bại\n", label);
        return;
    }

    for (JsonObject user : doc.as<JsonArray>()) {
        String ucode = user["code"].as<String>();
        if (ucode != code) continue;

        String dataB64 = user["fingerprint"].as<String>();
        String uname   = user["name"].as<String>();

        Serial.printf("[%s] code=%s | data_length = %d\n", label, ucode.c_str(), dataB64.length());

        if (dataB64.length() < 100) {
            Serial.printf("[%s] User %s không có template\n", label, ucode.c_str());
            return;
        }

        if (sensorType == 0) {
            int slot = prefs.getInt(("as_slot_" + code).c_str(), -1);
            if (slot == -1) slot = findFreeSlotAS();
            if (slot == -1) { Serial.println("[AS608] Cảm biến đầy"); return; }

            uint8_t result = uploadFingerprintTemplateAS((uint16_t)slot, dataB64);
            if (result == FINGERPRINT_OK) {
                prefs.putString(("as_user_" + String(slot)).c_str(), uname);
                prefs.putString(("as_code_" + String(slot)).c_str(), ucode);
                prefs.putInt(("as_slot_" + ucode).c_str(), slot);
                Serial.printf("[AS608] slot #%d OK\n", slot);
            }
        } else {
            int slot = prefs.getInt(("rs_slot_" + ucode).c_str(), -1);
            if (slot == -1) slot = findFreeSlotRS();
            if (slot == -1) { Serial.println("[R503] Cảm biến đầy"); return; }

            uint8_t result = uploadFingerprintTemplateRS((uint16_t)slot, dataB64);
            if (result == FINGERPRINT_OK) {
                prefs.putString(("rs_user_" + String(slot)).c_str(), uname);
                prefs.putString(("rs_code_" + String(slot)).c_str(), ucode);
                prefs.putInt(("rs_slot_" + ucode).c_str(), slot);
                Serial.printf("[R503] slot #%d SYNC THÀNH CÔNG\n", slot);
            } else {
                Serial.printf("[R503] Nạp template thất bại slot=%d (code=%s)\n", slot, ucode.c_str());
            }
        }
        return;
    }

    Serial.printf("syncUser [%s]: không tìm thấy code=%s\n", label, code.c_str());
}

// =========================================
// XÓA TOÀN BỘ R503
// =========================================
void emptyDatabaseRS() {
    Serial.println("[R503] Đang xóa toàn bộ thư viện vân tay...");
    if (!r503.emptyDatabase()) {
        Serial.println("[R503] ❌ Xóa database cảm biến thất bại");
        return;
    }
    Serial.println("[R503] ✅ ĐÃ XÓA SẠCH DATABASE cảm biến!");

    // Xóa toàn bộ Preferences liên quan R503 để đồng bộ với sensor
    for (int slot = 0; slot <= 199; slot++) {
        String nameKey = "rs_user_" + String(slot);
        String codeKey = "rs_code_" + String(slot);
        if (prefs.isKey(nameKey.c_str())) {
            String code = prefs.getString(codeKey.c_str(), "");
            prefs.remove(nameKey.c_str());
            prefs.remove(codeKey.c_str());
            if (code.length() > 0) prefs.remove(("rs_slot_" + code).c_str());
        }
    }
    Serial.println("[R503] ✅ ĐÃ XÓA SẠCH Preferences!");
}

// =========================================
// XÓA 1 TEMPLATE R503
// =========================================
bool deleteR503Template(uint16_t slot) {
    return r503.deleteTemplate(slot);
}

// =========================================
// XÓA TOÀN BỘ AS608
// =========================================
void emptyDatabaseAS() {
    Serial.println("[AS608] Đang xóa toàn bộ thư viện vân tay...");
    uint8_t result = fingerAS.emptyDatabase();

    if (result == FINGERPRINT_OK) {
        Serial.println("[AS608] THÀNH CÔNG: Đã xóa sạch mọi dấu vân tay trên cảm biến!");
    } else if (result == FINGERPRINT_PACKETRECIEVEERR) {
        Serial.println("[AS608] LỖI: Lỗi giao tiếp UART!");
        return;
    } else if (result == FINGERPRINT_DBCLEARFAIL) {
        Serial.println("[AS608] LỖI: Không thể xóa dữ liệu trên cảm biến!");
        return;
    } else {
        Serial.printf("[AS608] LỖI không xác định: 0x%02X\n", result);
        return;
    }

    // Xóa toàn bộ Preferences liên quan AS608 để đồng bộ với sensor
    for (int slot = 1; slot <= 127; slot++) {
        String nameKey = "as_user_" + String(slot);
        String codeKey = "as_code_" + String(slot);
        if (prefs.isKey(nameKey.c_str())) {
            String code = prefs.getString(codeKey.c_str(), "");
            prefs.remove(nameKey.c_str());
            prefs.remove(codeKey.c_str());
            prefs.remove("as_slot_5397");
            if (code.length() > 0) prefs.remove(("as_slot_" + code).c_str());
        }
    }
    Serial.println("[AS608] ✅ ĐÃ XÓA SẠCH Preferences!");
}