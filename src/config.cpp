#include "config.h"
#include "globals.h"
#include "secrets.h"

const char* ssid     = WIFI_SSID;
const char* password = WIFI_PASSWORD;

// FIX: getServerBase() đọc từ Preferences thay vì luôn dùng #define
String getServerBase() {
    String ip = prefs.getString("server_ip", DEFAULT_SERVER_IP);
    return "http://" + ip + ":" + String(SERVER_PORT);
}

void setServerIP(String ip) {
    prefs.putString("server_ip", ip);
    Serial.println("[config] Server IP đã lưu: " + ip);
}

// Khởi tạo secrets vào Preferences nếu chưa có (chạy 1 lần duy nhất)
void initSecrets() {
    if (!prefs.isKey("unlock_token")) {
        prefs.putString("unlock_token", UNLOCK_TOKEN);
        Serial.println("[config] Đã lưu unlock_token vào flash");
    }
    if (!prefs.isKey("api_key")) {
        prefs.putString("api_key", API_KEY);
        Serial.println("[config] Đã lưu api_key vào flash");
    }
}