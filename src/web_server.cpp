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
// HELPER — Gửi JSON log (mới nhất trước)
// =========================================
static void sendLogJson(AccessLog* log, int logIndex) {
    JsonDocument doc;
    JsonArray arr = doc.to<JsonArray>();

    int count = min(logIndex, 20);

    // Duyệt từ entry mới nhất (logIndex-1) về entry cũ nhất
    for (int i = 0; i < count; i++) {
        int idx = (logIndex - 1 - i + 20) % 20;
        char buffer[30];
        struct tm* timeinfo = localtime(&log[idx].timestamp);
        strftime(buffer, sizeof(buffer), "%H:%M:%S - %d/%m/%Y", timeinfo);

        JsonObject entry = arr.add<JsonObject>();
        entry["slot"]    = log[idx].id;
        entry["uid"]     = log[idx].uid;
        entry["name"]    = log[idx].name;
        entry["granted"] = log[idx].granted;
        entry["time"]    = String(buffer);
    }

    String json;
    serializeJson(doc, json);
    server.send(200, "application/json", json);
}

// =========================================
// HELPER — Gửi JSON users
// =========================================
static void sendUsersJson(const String& prefix) {
    JsonDocument doc;
    JsonArray arr = doc.to<JsonArray>();

    for (int slot = 0; slot <= 199; slot++) {
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
    // AS608 — ENROLL (yêu cầu API key)
    // =========================================
    server.on("/as608/enroll", HTTP_POST, []() {
        REQUIRE_API_KEY();
        String name = server.arg("name");
        String code = server.arg("code");
        if (name.isEmpty() || code.isEmpty()) { server.send(400, "text/plain", "Thiếu name/code"); return; }

        if (isEnrollingAS) {
            server.send(400, "text/plain", "[AS608] Đang enroll rồi!");
            return;
        }

        // Kiểm tra code đã tồn tại chưa
        if (prefs.getInt(("as_slot_" + code).c_str(), -1) != -1) {
            server.send(400, "text/plain", "[AS608] Mã số đã tồn tại, xóa trước rồi enroll lại");
            return;
        }

        isEnrollingAS = true;
        uint8_t result = enrollFinger(name, code, 0);
        isEnrollingAS = false;

        if (result != 0xFF && result != FINGERPRINT_TIMEOUT && result != FINGERPRINT_BADLOCATION) {
            server.send(200, "text/plain", "[AS608] Thêm thành công (slot " + String(result) + ")");
        } else {
            server.send(500, "text/plain", "[AS608] Enroll thất bại: " + String(result));
        }
    });

    // AS608 — DANH SÁCH USER (public)
    server.on("/as608/users", HTTP_GET, []() {
        sendUsersJson("as_");
    });

    // AS608 — XÓA USER (yêu cầu API key)
    server.on("/as608/delete", HTTP_POST, []() {
        REQUIRE_API_KEY();
        int slot = server.arg("slot").toInt();
        String code = server.arg("code");

        if (slot < 1 || slot > 127) { server.send(400, "text/plain", "Slot không hợp lệ"); return; }

        fingerAS.deleteModel(slot);
        prefs.remove(("as_user_" + String(slot)).c_str());
        prefs.remove(("as_code_" + String(slot)).c_str());
        if (code.length() > 0) prefs.remove(("as_slot_" + code).c_str());

        server.send(200, "text/plain", "[AS608] Đã xóa slot " + String(slot));
    });

    // AS608 — CẬP NHẬT TÊN (yêu cầu API key)
    server.on("/as608/update", HTTP_POST, []() {
        REQUIRE_API_KEY();
        int slot    = server.arg("slot").toInt();
        String name = server.arg("name");
        if (slot < 1 || slot > 127) { server.send(400, "text/plain", "Slot không hợp lệ"); return; }
        prefs.putString(("as_user_" + String(slot)).c_str(), name);
        server.send(200, "text/plain", "[AS608] Đã cập nhật");
    });

    // AS608 — LOG (public)
    server.on("/as608/log", HTTP_GET, []() {
        sendLogJson(logAS, logIndexAS);
    });

    // AS608 — SYNC TỪ DB (yêu cầu API key)
    server.on("/as608/sync-from-db", HTTP_POST, []() {
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
            server.send(200, "text/plain", "[AS608] Đang sync code=" + targetCode);
            syncUserFromServer(targetCode, 0);
        } else {
            server.send(200, "text/plain", "[AS608] Đang đồng bộ tất cả...");
            syncFromServer(0);
        }
    });

    // =========================================
    // R503 — ENROLL (yêu cầu API key)
    // =========================================
    server.on("/r503/enroll", HTTP_POST, []() {
        REQUIRE_API_KEY();
        String name = server.arg("name");
        String code = server.arg("code");
        if (name.isEmpty() || code.isEmpty()) { server.send(400, "text/plain", "Thiếu name/code"); return; }

        if (isEnrollingRS) {
            server.send(400, "text/plain", "[R503] Đang enroll rồi!");
            return;
        }

        if (prefs.getInt(("rs_slot_" + code).c_str(), -1) != -1) {
            server.send(400, "text/plain", "[R503] Mã số đã tồn tại, xóa trước rồi enroll lại");
            return;
        }

        isEnrollingRS = true;
        uint8_t result = enrollFinger(name, code, 1);
        isEnrollingRS = false;

        if (result != 0xFF && result != FINGERPRINT_TIMEOUT && result != FINGERPRINT_BADLOCATION) {
            server.send(200, "text/plain", "[R503] Thêm thành công (slot " + String(result) + ")");
        } else {
            server.send(500, "text/plain", "[R503] Enroll thất bại: " + String(result));
        }
    });
    
    // R503 — DANH SÁCH USER (public)  ← ĐÃ THÊM
    server.on("/r503/users", HTTP_GET, []() {
        sendUsersJson("rs_");
    });

    // R503 — DANH SÁCH USER (public)
    server.on("/r503/delete", HTTP_POST, []() {
        REQUIRE_API_KEY();
        int slot    = server.arg("slot").toInt();
        String code = server.arg("code");

        if (slot < 0 || slot > 199) { 
            server.send(400, "text/plain", "Slot không hợp lệ"); 
            return; 
        }

        // Xóa trên cảm biến R503
        if (r503.deleteTemplate(slot)) {
            // Xóa thông tin Preferences
            prefs.remove(("rs_user_" + String(slot)).c_str());
            prefs.remove(("rs_code_" + String(slot)).c_str());
            if (code.length() > 0) prefs.remove(("rs_slot_" + code).c_str());

            Serial.printf("[R503] ✅ Đã xóa slot %d hoàn tất\n", slot);
            server.send(200, "text/plain", "[R503] Đã xóa slot " + String(slot));
        } else {
            server.send(500, "text/plain", "[R503] Xóa template trên cảm biến thất bại");
        }
    });

    // R503 — CẬP NHẬT TÊN (yêu cầu API key)
    server.on("/r503/update", HTTP_POST, []() {
        REQUIRE_API_KEY();
        int slot    = server.arg("slot").toInt();
        String name = server.arg("name");
        if (slot < 1 || slot > 127) { server.send(400, "text/plain", "Slot không hợp lệ"); return; }
        prefs.putString(("rs_user_" + String(slot)).c_str(), name);
        server.send(200, "text/plain", "[R503] Đã cập nhật");
    });

    // R503 — LOG (public)
    server.on("/r503/log", HTTP_GET, []() {
        sendLogJson(logRS, logIndexRS);
    });

    // R503 — SYNC TỪ DB (yêu cầu API key)
    server.on("/r503/sync-from-db", HTTP_POST, []() {
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
            server.send(200, "text/plain", "[R503] Đang sync code=" + targetCode);
            syncUserFromServer(targetCode, 1);
        } else {
            server.send(200, "text/plain", "[R503] Đang đồng bộ tất cả...");
            syncFromServer(1);
        }
    });

    // =========================================
    // SYNC STATUS — public
    // =========================================
    server.on("/sync-status", HTTP_GET, []() {
        int countAS = 0, countRS = 0;
        for (int i = 1; i <= 127; i++) {
            if (prefs.isKey(("as_user_" + String(i)).c_str())) countAS++;
            if (prefs.isKey(("rs_user_" + String(i)).c_str())) countRS++;
        }
        JsonDocument doc;
        doc["as608"] = countAS;
        doc["r503"]  = countRS;
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
            logAccessAS(255, "", "Remote", true, "");
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
    // LOG PROXY
    // =========================================
    server.on("/log-proxy", HTTP_GET, []() {
        String sensor = server.arg("sensor");
        if (sensor == "as608") {
            sendLogJson(logAS, logIndexAS);
        } else if (sensor == "r503") {
            sendLogJson(logRS, logIndexRS);
        } else {
            if (WiFi.status() == WL_CONNECTED) {
                HTTPClient http;
                http.begin(getServerBase() + "/api/log?sensor=" + sensor);
                int code = http.GET();
                if (code == 200) {
                    server.send(200, "application/json", http.getString());
                } else {
                    server.send(500, "text/plain", "Proxy log error");
                }
                http.end();
            } else {
                server.send(503, "text/plain", "No WiFi");
            }
        }
    });

    server.begin();
}