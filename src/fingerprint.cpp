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
// R558S — GIAO TIẾP UART THÔ
// =========================================
static void r558s_sendCommand(uint8_t cmdCode, const uint8_t* params, uint16_t paramLen) {
    while (fpSerialRS.available()) fpSerialRS.read();
    delay(30);

    uint8_t packet[128];
    uint16_t idx = 0;

    packet[idx++] = 0xEF; packet[idx++] = 0x01;
    packet[idx++] = 0xFF; packet[idx++] = 0xFF;
    packet[idx++] = 0xFF; packet[idx++] = 0xFF;
    packet[idx++] = 0x01;

    uint16_t length = 3 + paramLen;
    packet[idx++] = (length >> 8) & 0xFF;
    packet[idx++] = length & 0xFF;

    packet[idx++] = cmdCode;

    // FIX: params có thể NULL nếu paramLen == 0 — kiểm tra trước khi dereference
    for (uint16_t i = 0; i < paramLen && params != nullptr; i++) {
        packet[idx++] = params[i];
    }

    uint16_t sum = 0;
    for (uint16_t i = 6; i < idx; i++) sum += packet[i];

    packet[idx++] = (sum >> 8) & 0xFF;
    packet[idx++] = sum & 0xFF;

    fpSerialRS.write(packet, idx);
    Serial.printf("[SEND] CMD=0x%02X | Len=%d | Checksum=0x%04X\n", cmdCode, idx, sum);
}

// FIX: Tách rõ tham số bufSize và timeout — trước đây bị nhầm lẫn
static bool r558s_readPacket(uint8_t* buf, size_t bufSize, uint32_t timeout = 3000) {
    uint32_t startTime = millis();
    size_t idx = 0;

    while (millis() - startTime < timeout) {
        if (fpSerialRS.available()) {
            uint8_t c = fpSerialRS.read();

            if (idx == 0) {
                if (c != 0xEF) continue;
            } else if (idx == 1) {
                if (c != 0x01) { idx = 0; continue; }
            }

            if (idx >= bufSize) {
                Serial.println("[R558S] Packet overflow!");
                return false;
            }

            buf[idx++] = c;

            if (idx >= 9) {
                uint16_t packetLen = ((uint16_t)buf[7] << 8) | (uint16_t)buf[8];
                uint16_t totalLen  = packetLen + 9;

                if (totalLen > bufSize) {
                    Serial.printf("[R558S] Packet too large: %d > %d\n", totalLen, (int)bufSize);
                    return false;
                }

                if (idx >= totalLen) return true;
            }
        }
        delay(1);
    }

    Serial.println("[R558S] Read packet timeout");
    return false;
}

static void r558s_setLED(uint8_t mode, uint8_t color) {
    uint8_t params[] = {mode, color, color, 0x00};
    r558s_sendCommand(0x3C, params, 4);
    delay(50);
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
    for (int slot = 1; slot <= 127; slot++) {
        if (!prefs.isKey(("rs_user_" + String(slot)).c_str())) return slot;
    }
    return -1;
}

// =========================================
// AS608 — UpChar command (nội bộ)
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
// LƯU FLASH + SYNC SERVER SAU ENROLL AS608
// =========================================
void syncAfterEnrollAS(uint8_t slot, String name, String code) {
    prefs.putString(("as_user_" + String(slot)).c_str(), name);
    prefs.putString(("as_code_" + String(slot)).c_str(), code);
    prefs.putInt(("as_slot_" + code).c_str(), slot);

    Serial.printf("[AS608] Đã lưu flash: slot=%d code=%s name=%s\n", slot, code.c_str(), name.c_str());

    if (WiFi.status() != WL_CONNECTED) return;

    uint8_t templateBuffer[512];
    if (manualDownloadModelAS(templateBuffer) != FINGERPRINT_OK) {
        Serial.println("[AS608] Download template thất bại — vẫn lưu flash OK");
        // Đẩy thông tin cơ bản không có template
        HTTPClient http;
        http.begin(getServerBase() + "/api/sync-user");
        http.addHeader("Content-Type", "application/json");
        JsonDocument doc;
        doc["slot"]   = slot;
        doc["name"]   = name;
        doc["code"]   = code;
        doc["sensor"] = "AS608";
        String payload; serializeJson(doc, payload);
        http.POST(payload); http.end();
        return;
    }

    String base64FP = base64Encode(templateBuffer, 512);

    HTTPClient http;
    http.begin(getServerBase() + "/api/sync-user");
    http.addHeader("Content-Type", "application/json");

    JsonDocument doc;
    doc["slot"]        = slot;
    doc["name"]        = name;
    doc["code"]        = code;
    doc["sensor"]      = "AS608";
    doc["fingerprint"] = base64FP;

    String payload;
    serializeJson(doc, payload);

    int httpCode = http.POST(payload);
    Serial.printf("[AS608] syncAfterEnroll: HTTP %d\n", httpCode);
    http.end();
}

// =========================================
// SYNC SAU ENROLL R558S — ĐÃ TỐI ƯU
// =========================================
void syncAfterEnrollRS(uint8_t slot, String name, String code) {
    prefs.putString(("rs_user_" + String(slot)).c_str(), name);
    prefs.putString(("rs_code_" + String(slot)).c_str(), code);
    prefs.putInt(("rs_slot_" + code).c_str(), slot);

    Serial.printf("[R558S] Enroll OK - slot=%d | code=%s | name=%s\n", 
                  slot, code.c_str(), name.c_str());

    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[R558S] Không có WiFi, chỉ lưu local");
        return;
    }

    // Thử lấy template
    uint8_t templateBuffer[1024] = {0};
    uint8_t result = manualDownloadModelRS(slot, templateBuffer);

    HTTPClient http;
    http.begin(getServerBase() + "/api/sync-user");
    http.addHeader("Content-Type", "application/json");

    JsonDocument doc;
    doc["slot"]   = slot;
    doc["name"]   = name;
    doc["code"]   = code;
    doc["sensor"] = "R558S";

    if (result == FINGERPRINT_OK) {
        String b64 = base64Encode(templateBuffer, 1024);
        doc["fingerprint"] = b64;
        Serial.printf("[R558S] Đẩy template (%d bytes)\n", b64.length());
    } else {
        Serial.println("[R558S] Không lấy được template → chỉ đẩy thông tin cơ bản");
    }

    String payload;
    serializeJson(doc, payload);
    int httpCode = http.POST(payload);
    Serial.printf("[R558S] syncAfterEnroll: HTTP %d\n", httpCode);
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
// ENROLL R558S
// =========================================
uint8_t enrollR558S(uint8_t slot, String name, String code) {
    uint16_t r558sSlot = slot - 1;
    Serial.printf("[R558S] AutoEnroll → slot #%d (r558s idx: %d)\n", slot, r558sSlot);
    r558s_setLED(1, 1);

    uint8_t params[] = {
        (uint8_t)(r558sSlot >> 8),
        (uint8_t)(r558sSlot & 0xFF),
        0x04,
        0x00,
        0x00
    };
    r558s_sendCommand(0x31, params, 5);

    uint8_t buf[32];
    while (true) {
        if (r558s_readPacket(buf, sizeof(buf), 20000)) {
            uint8_t confCode = buf[9];
            uint8_t step     = buf[10];
            Serial.printf("[R558S] step=0x%02X confCode=0x%02X\n", step, confCode);

            if (confCode != 0x00) {
                if (confCode == 0x27) Serial.println("[R558S] Lỗi: ID này đã có vân tay!");
                else Serial.printf("[R558S] Lỗi enroll: 0x%02X\n", confCode);
                r558s_setLED(3, 4); delay(1000); r558s_setLED(4, 0);
                pixels.clear(); pixels.show();
                return 0xFF;
            }

            if      (step == 0x01) Serial.println("[R558S] Đã chụp ảnh...");
            else if (step == 0x02) Serial.println("[R558S] Trích xuất đặc trưng OK");
            else if (step == 0x03) {
                Serial.println("[R558S] Nhấc tay ra rồi đặt lại...");
                r558s_setLED(3, 1); delay(100); r558s_setLED(1, 1);
            }
            else if (step == 0x06) {
                Serial.println("[R558S] Vân tay đã lưu!");
                r558s_setLED(3, 2); delay(1000); r558s_setLED(4, 0);
                syncAfterEnrollRS(slot, name, code);
                pixels.clear(); pixels.show();
                return slot;
            }
        } else {
            Serial.println("[R558S] Enroll timeout!");
            r558s_setLED(4, 0);
            pixels.clear(); pixels.show();
            return FINGERPRINT_TIMEOUT;
        }
    }
}

// =========================================
// ENROLL CHUNG
// sensorType: 0 = AS608, 1 = R558S
// =========================================
uint8_t enrollFinger(String name, String code, uint8_t sensorType) {
    pixels.setPixelColor(0, pixels.Color(255, 255, 0));
    pixels.show();

    if (sensorType == 0) {
        int slot = findFreeSlotAS();
        if (slot == -1) { Serial.println("[AS608] Cảm biến đã đầy"); return FINGERPRINT_BADLOCATION; }
        fingerAS.deleteModel(slot);
        Serial.printf("[AS608] Enrolling slot #%d | Name: %s | Code: %s\n", slot, name.c_str(), code.c_str());
        return enrollAS608((uint8_t)slot, name, code);
    } else {
        int slot = findFreeSlotRS();
        if (slot == -1) { Serial.println("[R558S] Cảm biến đã đầy"); return FINGERPRINT_BADLOCATION; }
        Serial.printf("[R558S] Enrolling slot #%d | Name: %s | Code: %s\n", slot, name.c_str(), code.c_str());
        return enrollR558S((uint8_t)slot, name, code);
    }
}

// =========================================
// DOWNLOAD TEMPLATE — AS608
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
            fpSerialAS.read(); // address bytes
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
        // checksum (2 bytes)
        for (int i = 0; i < 2; i++) {
            t = millis();
            while (!fpSerialAS.available() && millis() - t < 200);
            fpSerialAS.read();
        }
        if (pid == 0x08) break; // end packet
    }
    return FINGERPRINT_OK;
}

// =========================================
// DOWNLOAD TEMPLATE — R558S
// =========================================
// =========================================
// DOWNLOAD TEMPLATE R558S — ĐÃ SỬA
// =========================================
uint8_t manualDownloadModelRS(uint16_t slot, uint8_t* dest) {
    uint16_t r558sSlot = slot > 0 ? slot - 1 : 0;
    Serial.printf("[R558S] Download template slot %d\n", slot);

    // LoadChar trước
    uint8_t loadParams[] = {0x01, (uint8_t)(r558sSlot >> 8), (uint8_t)(r558sSlot & 0xFF)};
    r558s_sendCommand(0x07, loadParams, 3);

    uint8_t ack[32] = {0};
    if (!r558s_readPacket(ack, sizeof(ack), 2500) || ack[9] != 0x00) {
        Serial.println("[R558S] LoadChar thất bại");
        return 0xFF;
    }

    delay(300);

    // UpChar (0x08)
    uint8_t upParams[] = {0x01};   // buffer number 1
    r558s_sendCommand(0x08, upParams, 1);

    if (!r558s_readPacket(ack, sizeof(ack), 4000) || ack[9] != 0x00) {
        Serial.printf("[R558S] UpChar thất bại: 0x%02X\n", ack[9]);
        return 0xFF;
    }

    Serial.println("[R558S] UpChar OK → Đang nhận dữ liệu...");

    // Nhận các packet dữ liệu
    size_t total = 0;
    while (total < 1024) {
        uint8_t pkt[300] = {0};
        if (!r558s_readPacket(pkt, sizeof(pkt), 3500)) {
            Serial.println("[R558S] Timeout khi nhận data packet");
            break;
        }

        uint16_t pktLen = ((uint16_t)pkt[7] << 8) | pkt[8];
        uint16_t dataLen = pktLen - 2;

        for (uint16_t i = 0; i < dataLen && total < 1024; i++) {
            dest[total++] = pkt[9 + i];
        }

        if (pkt[6] == 0x08) break;   // packet cuối
    }

    Serial.printf("[R558S] Đọc template xong: %d bytes\n", total);
    return (total >= 512) ? FINGERPRINT_OK : 0xFF;
}

// =========================================
// UPLOAD TEMPLATE VÀO AS608
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

    // Gửi data packets (64 bytes mỗi packet theo AS608 spec)
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

    // Store vào slot
    uint8_t p = fingerAS.storeModel(slot);
    if (p != FINGERPRINT_OK) {
        Serial.printf("[AS608] StoreModel thất bại: 0x%02X\n", p);
        return 0xFF;
    }

    Serial.printf("[AS608] Upload template OK → slot #%d\n", slot);
    return FINGERPRINT_OK;
}

// =========================================
// UPLOAD TEMPLATE VÀO R558S
// =========================================
uint8_t uploadFingerprintTemplateRS(uint8_t slot, String base64Template) {
    uint16_t r558sSlot = slot > 0 ? slot - 1 : 0;

    Serial.printf("[R558S] Nạp Template từ Server vào slot %d\n", slot);

    const size_t MAX_TEMPLATE = 1024;
    uint8_t* templateData = (uint8_t*)malloc(MAX_TEMPLATE);
    if (!templateData) return 0xFF;

    size_t len = base64Decode(base64Template, templateData, MAX_TEMPLATE);

    if (len < 300) {
        Serial.printf("[R558S] Template không hợp lệ (len=%d)\n", (int)len);
        free(templateData);
        return 0xFF;
    }

    r558s_setLED(3, 1);

    // DownChar: gửi dữ liệu vào buffer 1 của cảm biến
    uint8_t downParams[] = {0x01};
    r558s_sendCommand(0x09, downParams, 1);

    uint8_t buf[32] = {0};
    if (!r558s_readPacket(buf, sizeof(buf), 2000) || buf[9] != 0x00) {
        Serial.println("[R558S] DownChar ACK thất bại");
        free(templateData);
        r558s_setLED(4, 0);
        return 0xFF;
    }

    // Gửi data packets (32 bytes/packet)
    const uint16_t PKT_SIZE = 32;
    int totalPkts = (len + PKT_SIZE - 1) / PKT_SIZE;

    for (int i = 0; i < totalPkts; i++) {
        uint8_t pid = (i == totalPkts - 1) ? 0x08 : 0x02;
        uint16_t dLen = (i == totalPkts - 1 && len % PKT_SIZE != 0) ? (len % PKT_SIZE) : PKT_SIZE;

        uint8_t pkt[64];
        int idx = 0;
        pkt[idx++] = 0xEF; pkt[idx++] = 0x01;
        pkt[idx++] = 0xFF; pkt[idx++] = 0xFF; pkt[idx++] = 0xFF; pkt[idx++] = 0xFF;
        pkt[idx++] = pid;

        uint16_t plen = dLen + 2;
        pkt[idx++] = plen >> 8;
        pkt[idx++] = plen & 0xFF;

        uint16_t sum = pid + (plen >> 8) + (plen & 0xFF);

        for (uint16_t j = 0; j < dLen; j++) {
            uint8_t b = templateData[i * PKT_SIZE + j];
            pkt[idx++] = b;
            sum += b;
        }
        pkt[idx++] = sum >> 8;
        pkt[idx++] = sum & 0xFF;

        fpSerialRS.write(pkt, idx);
        delay(8);
    }

    free(templateData);

    // Store vào slot
    uint8_t storeParams[] = {0x01, (uint8_t)(r558sSlot >> 8), (uint8_t)(r558sSlot & 0xFF)};
    r558s_sendCommand(0x06, storeParams, 3);

    if (!r558s_readPacket(buf, sizeof(buf), 3000) || buf[9] != 0x00) {
        Serial.println("[R558S] Store Template thất bại");
        r558s_setLED(4, 0);
        return 0xFF;
    }

    Serial.printf("[R558S] Nạp Template thành công slot %d\n", slot);
    r558s_setLED(2, 2);
    delay(800);
    r558s_setLED(4, 0);
    return FINGERPRINT_OK;
}

// =========================================
// CHECK FINGERPRINT — AS608
// =========================================
void checkAS608() {
    uint8_t p = fingerAS.getImage();
    if (p != FINGERPRINT_OK) return;

    p = fingerAS.image2Tz();
    if (p != FINGERPRINT_OK) return;

    p = fingerAS.fingerFastSearch();
    if (p == FINGERPRINT_OK) {
        uint8_t slot = fingerAS.fingerID;
        String name  = prefs.getString(("as_user_" + String(slot)).c_str(), "Unknown");
        String code  = prefs.getString(("as_code_" + String(slot)).c_str(), "");
        Serial.printf("[AS608] Match! Slot: %d | Code: %s | Name: %s | Confidence: %d\n",
                      slot, code.c_str(), name.c_str(), fingerAS.confidence);
        logAccessAS(slot, "", name, true, code);
        openLock();
    } else {
        Serial.println("[AS608] No match");
        logAccessAS(0, "", "Unknown", false, "");
        denyAccess();
    }
}

// =========================================
// CHECK FINGERPRINT — R558S
// =========================================
void checkR558S() {
    uint8_t params[] = {0x03, 0xFF, 0xFF, 0x00, 0x00};
    r558s_sendCommand(0x32, params, 5);

    uint8_t buf[32];
    if (!r558s_readPacket(buf, sizeof(buf), 2000)) return;

    uint8_t confCode = buf[9];
    uint8_t step     = buf[10];

    if (confCode == 0x00 && step == 0x05) {
        uint16_t matchedSlot = (buf[11] << 8) | buf[12];
        uint16_t score       = (buf[13] << 8) | buf[14];
        uint8_t flashSlot    = (uint8_t)(matchedSlot + 1);
        String name = prefs.getString(("rs_user_" + String(flashSlot)).c_str(), "Unknown");
        String code = prefs.getString(("rs_code_" + String(flashSlot)).c_str(), "");
        Serial.printf("[R558S] Match! Slot: %d | Code: %s | Name: %s | Score: %d\n",
                      flashSlot, code.c_str(), name.c_str(), score);
        logAccessRS(flashSlot, "", name, true, code);
        openLock();
    } else if (confCode == 0x09 || confCode == 0x24) {
        Serial.println("[R558S] No match");
        logAccessRS(0, "", "Unknown", false, "");
        denyAccess();
    }
    // step 0x01 = đang chờ ảnh → bỏ qua
}

// =========================================
// CHECK FINGERPRINT — GỌI CẢ 2
// =========================================
void checkFingerprint() {
    checkAS608();
    checkR558S();
}

// =========================================
// SYNC TOÀN BỘ TỪ SERVER
// FIX: R558S branch đọc "fingerprint" thay vì "template"
// =========================================
void syncFromServer(uint8_t sensorType) {
    if (WiFi.status() != WL_CONNECTED) { Serial.println("syncFromServer: không có WiFi"); return; }

    const char* label = (sensorType == 0) ? "AS608" : "R558S";
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
        String ucode   = user["code"].as<String>();
        String uname   = user["name"].as<String>();
        // FIX: cả 2 sensor đều dùng key "fingerprint" — nhất quán với DB schema
        String tmplB64 = user["fingerprint"].as<String>();

        if (tmplB64.length() < 100) {
            Serial.printf("[%s] code=%s: không có template\n", label, ucode.c_str());
            continue;
        }

        if (sensorType == 0) {
            int slot = prefs.getInt(("as_slot_" + ucode).c_str(), -1);
            if (slot == -1) slot = findFreeSlotAS();
            if (slot == -1) { Serial.println("[AS608] Cảm biến đầy, dừng sync"); break; }

            Serial.printf("[AS608] Nạp code=%s (%s) → slot #%d...\n", ucode.c_str(), uname.c_str(), slot);
            uint8_t result = uploadFingerprintTemplateAS((uint16_t)slot, tmplB64);
            if (result == FINGERPRINT_OK) {
                prefs.putString(("as_user_" + String(slot)).c_str(), uname);
                prefs.putString(("as_code_" + String(slot)).c_str(), ucode);
                prefs.putInt(("as_slot_" + ucode).c_str(), slot);
                successCount++;
            } else {
                Serial.printf("[AS608] slot #%d THẤT BẠI: %d\n", slot, result);
            }
        } else {
            int slot = prefs.getInt(("rs_slot_" + ucode).c_str(), -1);
            if (slot == -1) slot = findFreeSlotRS();
            if (slot == -1) { Serial.println("[R558S] Cảm biến đầy, dừng sync"); break; }

            Serial.printf("[R558S] Nạp code=%s (%s) → slot #%d...\n", ucode.c_str(), uname.c_str(), slot);
            uint8_t result = uploadFingerprintTemplateRS(slot, tmplB64);
            if (result == FINGERPRINT_OK) {
                prefs.putString(("rs_user_" + String(slot)).c_str(), uname);
                prefs.putString(("rs_code_" + String(slot)).c_str(), ucode);
                prefs.putInt(("rs_slot_" + ucode).c_str(), slot);
                successCount++;
            } else {
                Serial.printf("[R558S] Nạp template thất bại code=%s\n", ucode.c_str());
            }
        }
        delay(50);
    }

    Serial.printf("=== SYNC XONG [%s]: %d/%d user ===\n", label, successCount, (int)users.size());
    pixels.clear(); pixels.show();
}

// =========================================
// SYNC 1 USER TỪ SERVER
// FIX: R558S branch đọc "fingerprint" thay vì "template"
// =========================================
void syncUserFromServer(String code, uint8_t sensorType) {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("syncUser: không có WiFi");
        return;
    }

    const char* label = (sensorType == 0) ? "AS608" : "R558S";
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

    bool found = false;

    for (JsonObject user : doc.as<JsonArray>()) {
        String ucode = user["code"].as<String>();
        String uname = user["name"].as<String>();

        if (ucode != code) continue;
        found = true;

        // FIX: cả AS608 lẫn R558S đều đọc "fingerprint"
        String tmplB64 = user["fingerprint"].as<String>();

        if (tmplB64.length() < 100) {
            Serial.printf("[%s] User %s không có fingerprint template\n", label, ucode.c_str());
            break;
        }

        if (sensorType == 0) {
            int slot = prefs.getInt(("as_slot_" + code).c_str(), -1);
            if (slot == -1) slot = findFreeSlotAS();
            if (slot == -1) { Serial.println("[AS608] Cảm biến đầy"); break; }

            Serial.printf("[AS608] Nạp code=%s → slot #%d...\n", code.c_str(), slot);
            uint8_t result = uploadFingerprintTemplateAS((uint16_t)slot, tmplB64);

            if (result == FINGERPRINT_OK) {
                prefs.putString(("as_user_" + String(slot)).c_str(), uname);
                prefs.putString(("as_code_" + String(slot)).c_str(), ucode);
                prefs.putInt(("as_slot_" + ucode).c_str(), slot);
                Serial.printf("[AS608] slot #%d OK\n", slot);
            } else {
                Serial.printf("[AS608] slot #%d THẤT BẠI: %d\n", slot, result);
            }
        } else {
            int slot = prefs.getInt(("rs_slot_" + ucode).c_str(), -1);
            if (slot == -1) slot = findFreeSlotRS();
            if (slot == -1) { Serial.println("[R558S] Cảm biến đầy"); break; }

            Serial.printf("[R558S] Nạp Template code=%s → slot #%d...\n", ucode.c_str(), slot);
            uint8_t result = uploadFingerprintTemplateRS(slot, tmplB64);

            if (result == FINGERPRINT_OK) {
                prefs.putString(("rs_user_" + String(slot)).c_str(), uname);
                prefs.putString(("rs_code_" + String(slot)).c_str(), ucode);
                prefs.putInt(("rs_slot_" + ucode).c_str(), slot);
                Serial.printf("[R558S] slot #%d OK\n", slot);
            } else {
                Serial.printf("[R558S] Nạp Template thất bại code=%s\n", ucode.c_str());
            }
        }
        break;
    }

    if (!found) {
        Serial.printf("syncUser [%s]: không tìm thấy code=%s\n", label, code.c_str());
    }
}

// =========================================
// XÓA TOÀN BỘ R558S
// FIX: dùng mảng rỗng thay vì nullptr
// =========================================
void emptyR558S() {
    uint8_t emptyParams[1] = {0};  // FIX: không truyền nullptr
    r558s_sendCommand(0x0D, emptyParams, 0);

    uint8_t buf[32];
    if (r558s_readPacket(buf, sizeof(buf), 3000)) {
        if (buf[9] == 0x00) {
            Serial.println("[R558S] THÀNH CÔNG: Đã xóa toàn bộ vân tay!");
        } else {
            Serial.printf("[R558S] LỖI: Không thể xóa vân tay, mã lỗi: 0x%02X\n", buf[9]);
        }
    } else {
        Serial.println("[R558S] LỖI: Timeout khi chờ phản hồi xóa vân tay!");
    }
}

// =========================================
// XÓA TOÀN BỘ AS608
// =========================================
void emptyDatabaseAS() {
    Serial.println("[AS608] Đang xóa toàn bộ thư viện vân tay...");
    uint8_t result = fingerAS.emptyDatabase();

    if (result == FINGERPRINT_OK) {
        Serial.println("[AS608] THÀNH CÔNG: Đã xóa sạch mọi dấu vân tay!");
    } else if (result == FINGERPRINT_PACKETRECIEVEERR) {
        Serial.println("[AS608] LỖI: Lỗi giao tiếp UART!");
    } else if (result == FINGERPRINT_DBCLEARFAIL) {
        Serial.println("[AS608] LỖI: Không thể xóa dữ liệu trên cảm biến!");
    } else {
        Serial.printf("[AS608] LỖI không xác định: 0x%02X\n", result);
    }
}

// =========================================
// ĐỌC SYSTEM PARAMETER R558S
// =========================================
void r558s_readSysPara() {
    Serial.println("[R558S] Đang đọc System Parameter...");

    uint8_t cmd[] = {0xEF,0x01,0xFF,0xFF,0xFF,0xFF, 0x01,0x00,0x03,0x0F, 0x00,0x13};
    while (fpSerialRS.available()) fpSerialRS.read();
    fpSerialRS.write(cmd, sizeof(cmd));

    uint8_t buf[64] = {0};
    if (r558s_readPacket(buf, sizeof(buf), 2000)) {
        if (buf[9] == 0x00) {
            uint8_t packetSizeCode = buf[17];
            Serial.printf("[R558S] Packet Size code = %d (0=32B, 1=64B, 2=128B, 3=256B)\n", packetSizeCode);
        } else {
            Serial.printf("[R558S] ReadSysPara FAIL: 0x%02X\n", buf[9]);
        }
    } else {
        Serial.println("[R558S] ReadSysPara timeout");
    }
}