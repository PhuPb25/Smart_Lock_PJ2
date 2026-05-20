#include <Arduino.h>

#ifndef CONFIG_H
#define CONFIG_H

// Pin
#define SS_PIN    10
#define RST_PIN   9
#define RELAY_PIN 4

#define LOCK_OPEN_DURATION_MS 3000

// Server mặc định — lưu vào Preferences khi lần đầu boot
//#define DEFAULT_SERVER_IP "172.20.10.4"
#define DEFAULT_SERVER_IP "192.168.1.2"
#define SERVER_PORT       3000

// Lấy địa chỉ server hiện tại (đọc từ Preferences, fallback về DEFAULT)
String getServerBase();

// Lưu IP mới vào Preferences
void setServerIP(String ip);

// Khởi tạo token trong Preferences nếu chưa có
void initSecrets();

#endif