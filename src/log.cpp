#include "log.h"
#include "config.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// =========================================
// HELPER — Đẩy 1 log entry lên server
// sensor: "AS608" | "R558S" | "RFID" | "Remote"
// =========================================
static void pushLogToServer(int slot, const String& uid, const String& name,
                             const String& code, bool granted, const String& sensor) {
    if (WiFi.status() != WL_CONNECTED) return;

    HTTPClient http;
    http.begin(getServerBase() + "/api/log");
    http.addHeader("Content-Type", "application/json");

    JsonDocument doc;
    doc["slot"]    = slot;
    doc["uid"]     = uid;
    doc["name"]    = name;
    doc["code"]    = code;
    doc["granted"] = granted;
    doc["sensor"]  = sensor;

    String payload;
    serializeJson(doc, payload);

    int httpCode = http.POST(payload);
    if (httpCode != 200) {
        Serial.printf("[log] Push server thất bại: HTTP %d\n", httpCode);
    }
    http.end();
}

// =========================================
// AS608
// =========================================
AccessLog logAS[20];
int logIndexAS = 0;

void logAccessAS(int id, const String& uid, const String& name,
                 bool granted, const String& code) {
    logAS[logIndexAS % 20] = { id, uid, name, granted, time(nullptr) };
    logIndexAS++;
    pushLogToServer(id, uid, name, code, granted, "AS608");
}

// =========================================
// R558S
// =========================================
AccessLog logRS[20];
int logIndexRS = 0;

void logAccessRS(int id, const String& uid, const String& name,
                 bool granted, const String& code) {
    logRS[logIndexRS % 20] = { id, uid, name, granted, time(nullptr) };
    logIndexRS++;
    pushLogToServer(id, uid, name, code, granted, "R558S");
}