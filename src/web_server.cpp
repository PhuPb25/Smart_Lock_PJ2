#include "web_server.h"
#include "globals.h"
#include "config.h"
#include "index.h"
#include "rfid.h"
#include "fingerprint.h"
#include "lock_control.h"
#include "log.h"
#include <ArduinoJson.h>

// =========================================
// TRẠNG THÁI ENROLL — tách biệt cho 2 sensor
// =========================================
bool isEnrollingAS1 = false;
bool isEnrollingAS2 = false;

// ====================================================================
// HELPER — Hàm trung gian lấy Log từ Server DB và trả về cho Web Client
// ====================================================================
static void forwardLogFromServer(const String& sensorName) {
    if (WiFi.status() != WL_CONNECTED) {
        server.send(503, "application/json", "{\"error\":\"Mất kết nối WiFi\"}");
        return;
    }

    HTTPClient http;
    // Gọi đến API lấy log của Server Node.js kèm tham số lọc sensor
    http.begin(getServerBase() + "/api/log?sensor=" + sensorName);
    
    int httpCode = http.GET();
    if (httpCode == 200) {
        // Đọc toàn bộ chuỗi JSON trả về từ DB và gửi thẳng cho trình duyệt
        server.send(200, "application/json", http.getString());
    } else {
        Serial.printf("[Web] Lấy log từ DB thất bại, mã lỗi: %d\n", httpCode);
        server.send(httpCode, "application/json", "{\"error\":\"Không thể lấy log từ Database\"}");
    }
    http.end();
}

// =========================================
// HELPER — Gửi JSON users theo prefix Preferences
// =========================================
static void sendUsersJson(const String& prefix) {
    JsonDocument doc;
    JsonArray arr = doc.to<JsonArray>();

    for (int slot = 1; slot <= 127; slot++) {
        String key = prefix + "user_" + String(slot);
        if (prefs.isKey(key.c_str())) {
            String name = prefs.getString(key.c_str());
            String code = prefs.getString((prefix + "code_" + String(slot)).c_str(), "");

            JsonObject entry = arr.add<JsonObject>();
            entry["slot"] = slot;
            entry["code"] = code;
            entry["name"] = name;
        }
    }

    String json;
    serializeJson(doc, json);
    server.send(200, "application/json", json);
}

// =========================================
// MACRO — từ chối nếu thiếu API key
// =========================================
#define REQUIRE_API_KEY() do { if (!checkApiKey()) return; } while(0)

void setupWebServer() {

    const char* headers[] = {"X-API-Key"};
    server.collectHeaders(headers, 1);

    // =========================================
    // TRANG CHỦ — public
    // =========================================
    server.on("/", HTTP_GET, []() {
        server.send_P(200, "text/html", index_html);
    });

    // =========================================
    // RFID — SCAN (public)
    // =========================================
    server.on("/scan-rfid", HTTP_GET, []() {
        Serial.println("=== BAT DAU SCAN RFID ===");
        isScanningRFID = true;
        unsigned long start = millis();

        while (millis() - start < 5000) {
            if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()) {
                String uidStr = "";
                for (int i = 0; i < rfid.uid.size; i++) {
                    if (rfid.uid.uidByte[i] < 0x10) uidStr += "0";
                    uidStr += String(rfid.uid.uidByte[i], HEX);
                }
                uidStr.toUpperCase();
                rfid.PICC_HaltA();
                rfid.PCD_StopCrypto1();
                isScanningRFID = false;
                server.send(200, "text/plain", uidStr);
                return;
            }
        }

        isScanningRFID = false;
        server.send(200, "text/plain", "Timeout - Không thấy thẻ");
    });

    // =========================================
    // RFID — ADD (yêu cầu API key)
    // =========================================
    server.on("/add-rfid", HTTP_POST, []() {
        REQUIRE_API_KEY();
        if (!server.hasArg("uid")) { server.send(400, "text/plain", "Missing UID"); return; }

        String uidStr = server.arg("uid");
        String name   = server.arg("name");
        uidStr.toUpperCase();

        if (uidStr.length() != 8) { server.send(400, "text/plain", "UID phải 8 ký tự HEX"); return; }
        if (rfidCount >= MAX_RFID) { server.send(507, "text/plain", "Danh sách RFID đã đầy"); return; }

        saveRFID(uidStr, name);
        loadRFID();

        if (WiFi.status() == WL_CONNECTED) {
            HTTPClient http;
            http.begin(getServerBase() + "/api/rfid/sync");
            http.addHeader("Content-Type", "application/json");

            JsonDocument doc;
            doc["uid"]  = uidStr;
            doc["name"] = name;
            String payload;
            serializeJson(doc, payload);

            int code = http.POST(payload);
            Serial.printf("[add-rfid] sync server: %d\n", code);
            http.end();
        }

        server.send(200, "text/plain", "Đã thêm RFID");
    });

    // RFID — LIST (public)
    server.on("/rfid-list", HTTP_GET, []() {
        JsonDocument doc;
        JsonArray arr = doc.to<JsonArray>();
        for (int i = 0; i < rfidCount; i++) {
            JsonObject entry = arr.add<JsonObject>();
            entry["id"]   = i;
            entry["uid"]  = rfidUsers[i].uidStr;
            entry["name"] = rfidUsers[i].name;
        }
        String json; serializeJson(doc, json);
        server.send(200, "application/json", json);
    });

    // RFID — UPDATE (yêu cầu API key)
    server.on("/rfid-update", HTTP_POST, []() {
        REQUIRE_API_KEY();
        int id      = server.arg("id").toInt();
        String name = server.arg("name");
        if (id < 0 || id >= rfidCount) { server.send(400, "text/plain", "ID sai"); return; }
        rfidUsers[id].name = name;
        prefs.putString(("rfid_name_" + String(id)).c_str(), name);
        server.send(200, "text/plain", "Đã cập nhật RFID");
    });

    // RFID — DELETE (yêu cầu API key)
    server.on("/rfid-delete", HTTP_POST, []() {
        REQUIRE_API_KEY();
        int id = server.arg("id").toInt();
        if (id < 0 || id >= rfidCount) { server.send(400, "text/plain", "ID sai"); return; }

        for (int i = id; i < rfidCount - 1; i++) rfidUsers[i] = rfidUsers[i + 1];
        rfidCount--;

        for (int i = 0; i < MAX_RFID; i++) {
            prefs.remove(("rfid_uid_"  + String(i)).c_str());
            prefs.remove(("rfid_name_" + String(i)).c_str());
        }
        prefs.putInt("rfid_count", rfidCount);
        for (int i = 0; i < rfidCount; i++) {
            prefs.putString(("rfid_uid_"  + String(i)).c_str(), rfidUsers[i].uidStr);
            prefs.putString(("rfid_name_" + String(i)).c_str(), rfidUsers[i].name);
        }
        server.send(200, "text/plain", "Đã xóa RFID");
    });

    // =========================================
    // AS608 #1 — ENROLL (yêu cầu API key)
    // =========================================
    server.on("/as608-1/enroll", HTTP_POST, []() {
        REQUIRE_API_KEY();
        String name = server.arg("name");
        String code = server.arg("code");
        if (name.isEmpty() || code.isEmpty()) { server.send(400, "text/plain", "Thiếu name/code"); return; }

        if (isEnrollingAS1) {
            server.send(400, "text/plain", "[AS608-1] Đang enroll rồi!");
            return;
        }
        if (prefs.getInt(("as1_slot_" + code).c_str(), -1) != -1) {
            server.send(400, "text/plain", "[AS608-1] Mã số đã tồn tại trên cảm biến 1, xóa trước rồi enroll lại");
            return;
        }

        isEnrollingAS1 = true;
        uint8_t result = enrollFinger(name, code, 0);  // sensorIdx=0
        isEnrollingAS1 = false;

        if (result != 0xFF && result != FINGERPRINT_TIMEOUT && result != FINGERPRINT_BADLOCATION) {
            server.send(200, "text/plain", "[AS608-1] Thêm thành công (slot " + String(result) + ")");
        } else {
            server.send(500, "text/plain", "[AS608-1] Enroll thất bại: " + String(result));
        }
    });

    // AS608 #1 — DANH SÁCH USER (public)
    server.on("/as608-1/users", HTTP_GET, []() {
        sendUsersJson("as1_");
    });

    // AS608 #1 — XÓA USER (yêu cầu API key)
    server.on("/as608-1/delete", HTTP_POST, []() {
        REQUIRE_API_KEY();
        int slot    = server.arg("slot").toInt();
        String code = server.arg("code");

        if (slot < 1 || slot > 127) { server.send(400, "text/plain", "Slot không hợp lệ"); return; }

        fingerAS1.deleteModel(slot);
        prefs.remove(("as1_user_" + String(slot)).c_str());
        prefs.remove(("as1_code_" + String(slot)).c_str());
        if (code.length() > 0) prefs.remove(("as1_slot_" + code).c_str());

        server.send(200, "text/plain", "[AS608-1] Đã xóa slot " + String(slot));
    });

    // AS608 #1 — CẬP NHẬT TÊN (yêu cầu API key)
    server.on("/as608-1/update", HTTP_POST, []() {
        REQUIRE_API_KEY();
        int slot    = server.arg("slot").toInt();
        String name = server.arg("name");
        if (slot < 1 || slot > 127) { server.send(400, "text/plain", "Slot không hợp lệ"); return; }
        prefs.putString(("as1_user_" + String(slot)).c_str(), name);
        server.send(200, "text/plain", "[AS608-1] Đã cập nhật");
    });

    // AS608 #1 — LOG (public) -> Lấy từ DB
    server.on("/as608-1/log", HTTP_GET, []() {
        forwardLogFromServer("AS608-1");
    });

    // AS608 #1 — SYNC TỪ DB (yêu cầu API key)
    server.on("/as608-1/sync-from-db", HTTP_POST, []() {
        REQUIRE_API_KEY();
        if (WiFi.status() != WL_CONNECTED) { server.send(503, "text/plain", "Không có WiFi"); return; }

        String body = server.arg("plain");
        String targetCode = "";
        if (body.length() > 0) {
            JsonDocument doc;
            if (!deserializeJson(doc, body) && doc["code"].is<String>()) {
                targetCode = doc["code"].as<String>();
            }
        }

        if (targetCode.length() > 0) {
            server.send(200, "text/plain", "[AS608-1] Đang sync code=" + targetCode);
            syncUserFromServer(targetCode, 0);
        } else {
            server.send(200, "text/plain", "[AS608-1] Đang đồng bộ tất cả...");
            syncFromServer(0);
        }
    });

    // AS608 #1 — XÓA TOÀN BỘ (yêu cầu API key)
    server.on("/as608-1/clear", HTTP_POST, []() {
        REQUIRE_API_KEY();
        emptyDatabaseAS1();
        server.send(200, "text/plain", "[AS608-1] Đã xóa toàn bộ");
    });

    // =========================================
    // AS608 #2 — ENROLL (yêu cầu API key)
    // =========================================
    server.on("/as608-2/enroll", HTTP_POST, []() {
        REQUIRE_API_KEY();
        String name = server.arg("name");
        String code = server.arg("code");
        if (name.isEmpty() || code.isEmpty()) { server.send(400, "text/plain", "Thiếu name/code"); return; }

        if (isEnrollingAS2) {
            server.send(400, "text/plain", "[AS608-2] Đang enroll rồi!");
            return;
        }
        if (prefs.getInt(("as2_slot_" + code).c_str(), -1) != -1) {
            server.send(400, "text/plain", "[AS608-2] Mã số đã tồn tại trên cảm biến 2, xóa trước rồi enroll lại");
            return;
        }

        isEnrollingAS2 = true;
        uint8_t result = enrollFinger(name, code, 1);  // sensorIdx=1
        isEnrollingAS2 = false;

        if (result != 0xFF && result != FINGERPRINT_TIMEOUT && result != FINGERPRINT_BADLOCATION) {
            server.send(200, "text/plain", "[AS608-2] Thêm thành công (slot " + String(result) + ")");
        } else {
            server.send(500, "text/plain", "[AS608-2] Enroll thất bại: " + String(result));
        }
    });

    // AS608 #2 — DANH SÁCH USER (public)
    server.on("/as608-2/users", HTTP_GET, []() {
        sendUsersJson("as2_");
    });

    // AS608 #2 — XÓA USER (yêu cầu API key)
    server.on("/as608-2/delete", HTTP_POST, []() {
        REQUIRE_API_KEY();
        int slot    = server.arg("slot").toInt();
        String code = server.arg("code");

        if (slot < 1 || slot > 127) { server.send(400, "text/plain", "Slot không hợp lệ"); return; }

        fingerAS2.deleteModel(slot);
        prefs.remove(("as2_user_" + String(slot)).c_str());
        prefs.remove(("as2_code_" + String(slot)).c_str());
        if (code.length() > 0) prefs.remove(("as2_slot_" + code).c_str());

        server.send(200, "text/plain", "[AS608-2] Đã xóa slot " + String(slot));
    });

    // AS608 #2 — CẬP NHẬT TÊN (yêu cầu API key)
    server.on("/as608-2/update", HTTP_POST, []() {
        REQUIRE_API_KEY();
        int slot    = server.arg("slot").toInt();
        String name = server.arg("name");
        if (slot < 1 || slot > 127) { server.send(400, "text/plain", "Slot không hợp lệ"); return; }
        prefs.putString(("as2_user_" + String(slot)).c_str(), name);
        server.send(200, "text/plain", "[AS608-2] Đã cập nhật");
    });

    // AS608 #2 — LOG (public) -> Lấy từ DB
    server.on("/as608-2/log", HTTP_GET, []() {
        forwardLogFromServer("AS608-2");
    });

    // AS608 #2 — SYNC TỪ DB (yêu cầu API key)
    server.on("/as608-2/sync-from-db", HTTP_POST, []() {
        REQUIRE_API_KEY();
        if (WiFi.status() != WL_CONNECTED) { server.send(503, "text/plain", "Không có WiFi"); return; }

        String body = server.arg("plain");
        String targetCode = "";
        if (body.length() > 0) {
            JsonDocument doc;
            if (!deserializeJson(doc, body) && doc["code"].is<String>()) {
                targetCode = doc["code"].as<String>();
            }
        }

        if (targetCode.length() > 0) {
            server.send(200, "text/plain", "[AS608-2] Đang sync code=" + targetCode);
            syncUserFromServer(targetCode, 1);
        } else {
            server.send(200, "text/plain", "[AS608-2] Đang đồng bộ tất cả...");
            syncFromServer(1);
        }
    });

    // AS608 #2 — XÓA TOÀN BỘ (yêu cầu API key)
    server.on("/as608-2/clear", HTTP_POST, []() {
        REQUIRE_API_KEY();
        emptyDatabaseAS2();
        server.send(200, "text/plain", "[AS608-2] Đã xóa toàn bộ");
    });

    // =========================================
    // SYNC STATUS — public
    // =========================================
    server.on("/sync-status", HTTP_GET, []() {
        int countAS1 = 0, countAS2 = 0;
        for (int i = 1; i <= 127; i++) {
            if (prefs.isKey(("as1_user_" + String(i)).c_str())) countAS1++;
            if (prefs.isKey(("as2_user_" + String(i)).c_str())) countAS2++;
        }
        JsonDocument doc;
        doc["as608_1"] = countAS1;
        doc["as608_2"] = countAS2;
        String json; serializeJson(doc, json);
        server.send(200, "application/json", json);
    });

    // =========================================
    // MỞ KHÓA TỪ XA
    // =========================================
    server.on("/unlock", HTTP_POST, []() {
        String token       = server.arg("token");
        String storedToken = prefs.getString("unlock_token", "");

        if (storedToken.length() == 0) {
            server.send(500, "application/json", "{\"status\":\"token chưa được cấu hình\"}");
            return;
        }
        if (token == storedToken) {
            // Đẩy log trực tiếp lên DB Server thay vì gọi hàm log cục bộ
            if (WiFi.status() == WL_CONNECTED) {
                HTTPClient http;
                http.begin(getServerBase() + "/api/log");
                http.addHeader("Content-Type", "application/json");

                JsonDocument doc;
                doc["slot"]    = 255;
                doc["uid"]     = "";
                doc["name"]    = "Remote";
                doc["code"]    = "";
                doc["granted"] = true;
                doc["sensor"]  = "REMOTE";

                String payload;
                serializeJson(doc, payload);
                http.POST(payload);
                http.end();
            }
            
            openLock();
            server.send(200, "application/json", "{\"status\":\"unlocked\"}");
        } else {
            server.send(403, "application/json", "{\"status\":\"unauthorized\"}");
        }
    });

    // =========================================
    // PROXY DB — GET users (public)
    // =========================================
    server.on("/db-users", HTTP_GET, []() {
        if (WiFi.status() != WL_CONNECTED) {
            server.send(503, "text/plain", "Không có WiFi");
            return;
        }
        HTTPClient http;
        http.begin(getServerBase() + "/api/users/all");
        int code = http.GET();
        if (code == 200) {
            server.send(200, "application/json", http.getString());
        } else {
            server.send(500, "text/plain", "Lỗi kết nối server: " + String(code));
        }
        http.end();
    });

    // PROXY DB — POST update user name (yêu cầu API key)
    server.on("/db-users", HTTP_POST, []() {
        REQUIRE_API_KEY();
        if (WiFi.status() != WL_CONNECTED) { server.send(503, "text/plain", "Không có WiFi"); return; }

        String body = server.arg("plain");
        JsonDocument reqDoc;
        if (deserializeJson(reqDoc, body) || !reqDoc["code"].is<String>()) {
            server.send(400, "application/json", "{\"error\":\"Thiếu code\"}");
            return;
        }

        String code = reqDoc["code"].as<String>();
        String name = reqDoc["name"] | "";

        HTTPClient http;
        http.begin(getServerBase() + "/api/sync-user");
        http.addHeader("Content-Type", "application/json");

        JsonDocument sendDoc;
        sendDoc["code"] = code;
        sendDoc["name"] = name;
        String payload; serializeJson(sendDoc, payload);

        int httpCode = http.POST(payload);
        if (httpCode == 200) {
            server.send(200, "application/json", "{\"status\":\"ok\"}");
        } else {
            server.send(500, "application/json", "{\"error\":\"Server lỗi\"}");
        }
        http.end();
    });

    // PROXY DB — GET rfid (public)
    server.on("/db-rfid", HTTP_GET, []() {
        if (WiFi.status() != WL_CONNECTED) { server.send(503, "text/plain", "Không có WiFi"); return; }
        HTTPClient http;
        http.begin(getServerBase() + "/api/rfid");
        int code = http.GET();
        if (code == 200) server.send(200, "application/json", http.getString());
        else server.send(500, "text/plain", "Lỗi kết nối server");
        http.end();
    });

    // PROXY DB — POST update rfid name (yêu cầu API key)
    server.on("/db-rfid", HTTP_POST, []() {
        REQUIRE_API_KEY();
        if (WiFi.status() != WL_CONNECTED) { server.send(503, "text/plain", "Không có WiFi"); return; }

        String body = server.arg("plain");
        JsonDocument reqDoc;
        if (deserializeJson(reqDoc, body) || !reqDoc["uid"].is<String>()) {
            server.send(400, "application/json", "{\"error\":\"Thiếu uid\"}");
            return;
        }

        String uid  = reqDoc["uid"].as<String>();
        String name = reqDoc["name"] | "";

        HTTPClient http;
        http.begin(getServerBase() + "/api/rfid/sync");
        http.addHeader("Content-Type", "application/json");

        JsonDocument sendDoc;
        sendDoc["uid"]  = uid;
        sendDoc["name"] = name;
        String payload; serializeJson(sendDoc, payload);

        int httpCode = http.POST(payload);
        if (httpCode == 200) {
            server.send(200, "application/json", "{\"status\":\"ok\"}");
        } else {
            server.send(500, "application/json", "{\"error\":\"Server lỗi\"}");
        }
        http.end();
    });

    // =========================================
    // CẤU HÌNH SERVER IP (yêu cầu API key)
    // =========================================
    server.on("/set-server-ip", HTTP_POST, []() {
        REQUIRE_API_KEY();
        String ip = server.arg("plain");
        ip.trim();
        if (ip.length() < 7) { server.send(400, "text/plain", "IP không hợp lệ"); return; }
        setServerIP(ip);
        server.send(200, "text/plain", "Server IP đã cập nhật: " + ip);
    });

    // =========================================
    // LOG PROXY — Chuyển tiếp toàn bộ yêu cầu về DB Server
    // =========================================
    server.on("/log-proxy", HTTP_GET, []() {
        String sensor = server.arg("sensor");
        if (sensor.length() == 0) sensor = "all";
        forwardLogFromServer(sensor);
    });

    server.begin();
}