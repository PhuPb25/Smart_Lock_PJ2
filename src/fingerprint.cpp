#include "fingerprint.h"
#include "globals.h"
#include "config.h"
#include "lock_control.h"
#include "log.h"
#include "oled.h"          // ← OLED
#include <HTTPClient.h>
#include <ArduinoJson.h>

// =========================================
// BASE64
// =========================================
// Mã hóa Base64 và giải mã Base64
static const char b64chars[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"; // Chuỗi ký tự Base64

static String base64Encode(const uint8_t* data, size_t len) {
    String result = ""; // Chuỗi kết quả Base64
    result.reserve((len / 3 + 1) * 4 + 4); // Dự trữ bộ nhớ: 4 ký tự cho mỗi 3 byte + đệm
    for (size_t i = 0; i < len; i += 3) { // Duyệt dữ liệu theo khối 3 byte
        uint8_t b0 = data[i]; // Lấy byte 0 của khối
        uint8_t b1 = (i + 1 < len) ? data[i + 1] : 0; // Lấy byte 1 nếu có, ngược lại 0
        uint8_t b2 = (i + 2 < len) ? data[i + 2] : 0; // Lấy byte 2 nếu có, ngược lại 0
        result += b64chars[b0 >> 2]; // 6 bit cao của b0 -> ký tự Base64 1
        result += b64chars[((b0 & 0x03) << 4) | (b1 >> 4)]; // 2 bit thấp của b0 + 4 bit cao của b1 -> ký tự 2
        // Nếu có byte 1, tạo ký tự 3 từ 4 bit thấp của b1 và 2 bit cao của b2, nếu không gán '='
        result += (i + 1 < len) ? b64chars[((b1 & 0x0F) << 2) | (b2 >> 6)] : '=';
        // Nếu có byte 2, ký tự 4 lấy 6 bit thấp của b2, nếu không gán '='
        result += (i + 2 < len) ? b64chars[b2 & 0x3F] : '=';
    }
    return result; // Trả về chuỗi Base64
}

static int base64Decode(const String& input, uint8_t* output, size_t maxLen) { 
    // Hàm giải mã Base64: trả về số byte giải mã
    static const int8_t b64index[256] = {// Bảng ánh xạ ký tự Base64 -> giá trị 6-bit hoặc -1 nếu không hợp lệ
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
    size_t outLen = 0; // Số byte đã ghi vào output
    uint32_t buf  = 0;  // Bộ đệm bit tạm thời để gom các nhóm 6-bit
    int bits      = 0;  // Số bit hiện có trong buf
    for (int i = 0; i < (int)input.length(); i++) { // Duyệt từng ký tự trong input
        char c = input[i]; // Lấy ký tự hiện tại
        if (c == '=') break; // Dấu '=' là padding -> kết thúc chuỗi dữ liệu
        int8_t val = b64index[(uint8_t)c]; // Lấy giá trị 6-bit từ bảng, hoặc -1 nếu ký tự không phải Base64
        if (val < 0) continue; // Bỏ qua ký tự không hợp lệ (ví dụ ký tự xuống dòng)
        buf  = (buf << 6) | val; // Dịch buf 6 bit và ghép thêm giá trị mới
        bits += 6; // Tăng số bit có trong buf
        if (bits >= 8) { // Nếu có ít nhất 8 bit, trích một byte
            bits -= 8; // Giảm số bit còn lại sau khi trích
            if (outLen < maxLen) output[outLen++] = (uint8_t)(buf >> bits); // Ghi byte cao vào output nếu còn chỗ
        }
    }
    return (int)outLen; // Trả về số byte đã giải mã
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
static uint8_t sendUploadCharCommand(HardwareSerial& serial) { // Gửi lệnh UpChar thủ công tới cảm biến qua serial
    uint8_t cmd[] = { // Mảng byte lệnh theo giao thức của AS608
        0xEF, 0x01, 0xFF, 0xFF, 0xFF, 0xFF, // Header và address
        0x01, 0x00, 0x04, 0x09, 0x01,       // Packet type, length và mã lệnh UpChar
        0x00, 0x0F                          // Checksum (2 byte)
    };
    serial.write(cmd, sizeof(cmd)); // Gửi mảng lệnh ra serial
    uint32_t t = millis(); // Bắt đầu tính thời gian chờ
    while (serial.available() < 12 && millis() - t < 1000); // Chờ ít nhất 12 byte phản hồi hoặc timeout 1s
    uint8_t resp[12] = {0}; // Buffer chứa phản hồi tối đa 12 byte
    int n = 0; // Số byte đã đọc
    t = millis(); // Đặt lại bộ đếm thời gian
    while (n < 12 && millis() - t < 500) { // Đọc tối đa 12 byte hoặc timeout 500ms
        if (serial.available()) resp[n++] = serial.read(); // Nếu có dữ liệu, đọc vào buffer
    }
    for (int i = 0; i < n - 1; i++) { // Duyệt buffer để tìm header hợp lệ
        if (resp[i] == 0xEF && resp[i+1] == 0x01) { // Nếu tìm thấy header của gói
            return (resp[i + 9] == 0x00) ? FINGERPRINT_OK : resp[i + 9]; // Trả về FINGERPRINT_OK nếu code 0x00, ngược lại trả mã lỗi
        }
    }
    return FINGERPRINT_PACKETRECIEVEERR; // Nếu không nhận được gói hợp lệ, trả về lỗi nhận gói
}

// =========================================
// DOWNLOAD TEMPLATE — AS608 (512 bytes)
// =========================================
uint8_t manualDownloadModelAS(Adafruit_Fingerprint& finger, HardwareSerial& serial, uint8_t* dest) {
    uint8_t p = finger.getModel(); // Lấy model hiện tại từ cảm biến vân tay
    if (p != FINGERPRINT_OK) return p; // Nếu không thành công, trả về mã lỗi

    uint16_t bytesRead = 0; // Số byte đã đọc vào buffer dest
    uint32_t startTimer = millis(); // Bắt đầu bộ đếm thời gian chờ tổng
    while (bytesRead < 512) { // Tiếp tục cho đến khi đọc đủ 512 byte
        if (millis() - startTimer > 3000) return FINGERPRINT_TIMEOUT; // Nếu quá 3 giây, trả về timeout
        if (!serial.available()) continue; // Nếu không có dữ liệu, quay vòng tiếp
        if (serial.read() != 0xEF) continue; // Kiểm tra byte header, nếu sai thì bỏ qua
        uint32_t t = millis(); // Bắt đầu bộ đếm thời gian chờ cho mỗi byte tiếp theo
        while (!serial.available() && millis() - t < 200); // Chờ byte tiếp theo tối đa 200ms
        if (serial.read() != 0x01) continue; // Kiểm tra byte địa chỉ, nếu sai thì bỏ qua gói này
        for (int i = 0; i < 4; i++) { // Bỏ qua 4 byte địa chỉ tiếp theo
            t = millis();
            while (!serial.available() && millis() - t < 200);
            serial.read();
        }
        t = millis();
        while (!serial.available() && millis() - t < 200); // Chờ packet id
        uint8_t pid = serial.read(); // Đọc packet id
        t = millis();
        while (serial.available() < 2 && millis() - t < 200); // Chờ 2 byte độ dài
        uint16_t len = ((uint16_t)serial.read() << 8) | serial.read(); // Đọc độ dài gói
        uint16_t dataLen = len - 2; // Dữ liệu thực tế = độ dài - 2 byte checksum
        for (uint16_t i = 0; i < dataLen && bytesRead < 512; i++) { // Đọc payload dữ liệu
            t = millis();
            while (!serial.available() && millis() - t < 200);
            dest[bytesRead++] = serial.read(); // Lưu byte vào buffer đích
        }
        for (int i = 0; i < 2; i++) { // Bỏ qua 2 byte checksum
            t = millis();
            while (!serial.available() && millis() - t < 200);
            serial.read();
        }
        if (pid == 0x08) break; // Nếu đây là packet dữ liệu cuối, kết thúc vòng lặp
    }
    return FINGERPRINT_OK; // Trả về OK nếu đã đọc xong
}

// =========================================
// UPLOAD TEMPLATE VÀO AS608 (512 bytes)
// =========================================
uint8_t uploadFingerprintTemplate(Adafruit_Fingerprint& finger, HardwareSerial& serial, uint16_t slot, String base64Data) {
    uint8_t templateBuffer[512] = {0}; // Tạo buffer 512 byte để lưu template đã giải mã
    int len = base64Decode(base64Data, templateBuffer, 512); // Giải mã base64 và lưu vào buffer
    if (len < 256) { // Nếu dữ liệu sau giải mã nhỏ hơn 256 byte thì không hợp lệ
        Serial.println("[AS608] Template quá ngắn sau decode");
        return 0xFF; // Trả về lỗi
    }

    if (sendUploadCharCommand(serial) != FINGERPRINT_OK) { // Gửi lệnh bắt đầu upload template
        Serial.println("[AS608] UploadChar command thất bại");
        return 0xFF; // Nếu lệnh thất bại thì dừng
    }

    const uint16_t PKT_DATA = 64; // Kích thước dữ liệu mỗi packet là 64 byte
    int totalPkts = (len + PKT_DATA - 1) / PKT_DATA; // Tính số packet cần gửi

    for (int i = 0; i < totalPkts; i++) {
        uint8_t pid   = (i == totalPkts - 1) ? 0x08 : 0x02; // Packet cuối dùng id 0x08, các packet khác 0x02
        uint16_t dLen = (i == totalPkts - 1 && len % PKT_DATA != 0)
                        ? (len % PKT_DATA) : PKT_DATA; // Độ dài dữ liệu packet cuối có thể nhỏ hơn 64

        uint8_t pkt[80]; // Mảng chứa packet gửi đi
        int idx = 0; // Chỉ số viết vào packet
        pkt[idx++] = 0xEF; pkt[idx++] = 0x01; // Header cố định
        pkt[idx++] = 0xFF; pkt[idx++] = 0xFF; pkt[idx++] = 0xFF; pkt[idx++] = 0xFF; // Address mặc định
        pkt[idx++] = pid; // Packet ID
        uint16_t plen = dLen + 2; // Tổng độ dài payload + 2 byte checksum
        pkt[idx++] = plen >> 8; // Byte cao độ dài packet
        pkt[idx++] = plen & 0xFF; // Byte thấp độ dài packet

        uint16_t sum = pid + (plen >> 8) + (plen & 0xFF); // Tính checksum ban đầu
        for (uint16_t j = 0; j < dLen; j++) {
            uint8_t b = templateBuffer[i * PKT_DATA + j]; // Lấy byte dữ liệu từ template
            pkt[idx++] = b; // Ghi dữ liệu vào packet
            sum += b; // Cộng checksum
        }
        pkt[idx++] = sum >> 8; // Checksum byte cao
        pkt[idx++] = sum & 0xFF; // Checksum byte thấp

        serial.write(pkt, idx); // Gửi packet qua serial
        delay(5); // Đợi một chút để thiết bị nhận dữ liệu
    }

    uint8_t p = finger.storeModel(slot); // Gọi hàm lưu model vào slot của cảm biến
    if (p != FINGERPRINT_OK) {
        Serial.printf("[AS608] StoreModel thất bại: 0x%02X\n", p); // In lỗi nếu thất bại
        return 0xFF;
    }
    Serial.printf("[AS608] Upload template OK → slot #%d\n", slot); // Thông báo upload thành công
    return FINGERPRINT_OK; // Trả về thành công
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
    oledShow(OLED_ENROLLING, name);        // ← OLED: Bước 1
    pixels.setPixelColor(0, pixels.Color(255, 165, 0));
    pixels.show();

    uint32_t t = millis();
    while (fingerAS1.getImage() != FINGERPRINT_OK) {
        server.handleClient();
        oledLoop();                        // ← Giữ OLED responsive khi chờ
        if (millis() - t > 15000) {
            pixels.clear(); pixels.show();
            oledShow(OLED_ENROLL_FAIL, "Timeout - AS1");
            return FINGERPRINT_TIMEOUT;
        }
    }
    if (fingerAS1.image2Tz(1) != FINGERPRINT_OK) {
        pixels.clear(); pixels.show();
        oledShow(OLED_ENROLL_FAIL, "Anh mo - AS1");
        return FINGERPRINT_IMAGEMESS;
    }

    Serial.println("[AS1] Nhấc tay ra...");
    oledShow(OLED_ENROLL_STEP2, name);     // ← OLED: Yêu cầu quét lần 2
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
        oledLoop();
        if (millis() - t > 15000) {
            pixels.clear(); pixels.show();
            oledShow(OLED_ENROLL_FAIL, "Timeout lan 2");
            return FINGERPRINT_TIMEOUT;
        }
    }
    if (fingerAS1.image2Tz(2) != FINGERPRINT_OK) {
        pixels.clear(); pixels.show();
        oledShow(OLED_ENROLL_FAIL, "Anh mo lan 2");
        return FINGERPRINT_IMAGEMESS;
    }
    if (fingerAS1.createModel() != FINGERPRINT_OK) {
        pixels.clear(); pixels.show();
        oledShow(OLED_ENROLL_FAIL, "2 lan khong khop");
        return FINGERPRINT_ENROLLMISMATCH;
    }
    if (fingerAS1.storeModel(slot) != FINGERPRINT_OK) {
        pixels.clear(); pixels.show();
        oledShow(OLED_ENROLL_FAIL, "Loi ghi flash");
        return FINGERPRINT_BADLOCATION;
    }

    Serial.printf("[AS1] Enroll slot #%d OK\n", slot);
    oledShow(OLED_ENROLL_OK, name + " #" + String(slot));  // ← OLED: Thành công
    syncAfterEnroll(slot, name, code, 0);  // sensorIdx=0 → chỉ ghi as1_
    pixels.clear(); pixels.show();
    return slot;
}

// =========================================
// ENROLL AS2 — chỉ enroll trên AS2
// =========================================
uint8_t enrollAS2(uint8_t slot, String name, String code) {
    Serial.println("[AS2] Đặt ngón tay lên cảm biến 2...");
    oledShow(OLED_ENROLLING, name);        // ← OLED: Bước 1
    pixels.setPixelColor(0, pixels.Color(0, 165, 255));
    pixels.show();

    uint32_t t = millis();
    while (fingerAS2.getImage() != FINGERPRINT_OK) {
        server.handleClient();
        oledLoop();
        if (millis() - t > 15000) {
            pixels.clear(); pixels.show();
            oledShow(OLED_ENROLL_FAIL, "Timeout - AS2");
            return FINGERPRINT_TIMEOUT;
        }
    }
    if (fingerAS2.image2Tz(1) != FINGERPRINT_OK) {
        pixels.clear(); pixels.show();
        oledShow(OLED_ENROLL_FAIL, "Anh mo - AS2");
        return FINGERPRINT_IMAGEMESS;
    }

    Serial.println("[AS2] Nhấc tay ra...");
    oledShow(OLED_ENROLL_STEP2, name);     // ← OLED: Yêu cầu quét lần 2
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
        oledLoop();
        if (millis() - t > 15000) {
            pixels.clear(); pixels.show();
            oledShow(OLED_ENROLL_FAIL, "Timeout lan 2");
            return FINGERPRINT_TIMEOUT;
        }
    }
    if (fingerAS2.image2Tz(2) != FINGERPRINT_OK) {
        pixels.clear(); pixels.show();
        oledShow(OLED_ENROLL_FAIL, "Anh mo lan 2");
        return FINGERPRINT_IMAGEMESS;
    }
    if (fingerAS2.createModel() != FINGERPRINT_OK) {
        pixels.clear(); pixels.show();
        oledShow(OLED_ENROLL_FAIL, "2 lan khong khop");
        return FINGERPRINT_ENROLLMISMATCH;
    }
    if (fingerAS2.storeModel(slot) != FINGERPRINT_OK) {
        pixels.clear(); pixels.show();
        oledShow(OLED_ENROLL_FAIL, "Loi ghi flash");
        return FINGERPRINT_BADLOCATION;
    }

    Serial.printf("[AS2] Enroll slot #%d OK\n", slot);
    oledShow(OLED_ENROLL_OK, name + " #" + String(slot));  // ← OLED: Thành công
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
            Serial.printf("[AS1] Match! Slot:%d Code:%s Name:%s Score:%d\n",
                          id, code.c_str(), name.c_str(), score);
            oledShow(OLED_GRANTED, name);   // ← OLED
            logAccessAS1(id, "", name, true, code);
            openLock();
        } else {
            Serial.printf("[AS1] Tin cậy quá thấp (%d) → từ chối\n", fingerAS1.confidence);
            oledShow(OLED_DENIED, "AS1 - Thap diem");  // ← OLED
            logAccessAS1(0, "", "Unknown", false, "");
            denyAccess();
        }
    } else if (p == FINGERPRINT_NOTFOUND) {
        Serial.println("[AS1] Không khớp");
        oledShow(OLED_DENIED, "AS1 - Khong khop");     // ← OLED
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
            Serial.printf("[AS2] Match! Slot:%d Code:%s Name:%s Score:%d\n",
                          id, code.c_str(), name.c_str(), score);
            oledShow(OLED_GRANTED, name);   // ← OLED
            logAccessAS2(id, "", name, true, code);
            openLock();
        } else {
            Serial.printf("[AS2] Tin cậy quá thấp (%d) → từ chối\n", fingerAS2.confidence);
            oledShow(OLED_DENIED, "AS2 - Thap diem");  // ← OLED
            logAccessAS2(0, "", "Unknown", false, "");
            denyAccess();
        }
    } else if (p == FINGERPRINT_NOTFOUND) {
        Serial.println("[AS2] Không khớp");
        oledShow(OLED_DENIED, "AS2 - Khong khop");     // ← OLED
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
    oledShow(OLED_SYNCING, sensorName);    // ← OLED
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
    oledShow(OLED_IDLE);                   // ← OLED: Về IDLE sau khi sync xong
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
    Serial.println("[AS1] Xóa xong");
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
    Serial.println("[AS2] Xóa xong");
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