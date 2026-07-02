#include <Arduino.h>

#ifndef CONFIG_H
#define CONFIG_H

// =========================================
// PIN CẤU HÌNH
// =========================================
// AS608 #1 — UART1 (RX=17, TX=18)
#define AS1_RX_PIN  17
#define AS1_TX_PIN  18
// AS608 #2 — UART2 (RX=15, TX=16)
#define AS2_RX_PIN  15
#define AS2_TX_PIN  16

// RFID RC522
#define SS_PIN    10
#define RST_PIN   9
#define MOSI_PIN  11  
#define MISO_PIN  13
#define SCK       12

// =========================================
// CẤU HÌNH OLED
// Màn hình SSD1306 128x64, giao tiếp I2C
// SDA = GPIO 4, SCL = GPIO 5 (mặc định ESP32)
// =========================================
#define OLED_SDA_PIN   4
#define OLED_SCL_PIN   5
#define OLED_WIDTH    128
#define OLED_HEIGHT    64
#define OLED_ADDR    0x3C   // Địa chỉ I2C phổ biến; đổi 0x3D nếu chân SA0 = HIGH

#define RELAY_PIN 6

#define LOCK_OPEN_DURATION_MS 3000

// Server mặc định — lưu vào Preferences khi lần đầu boot
#define DEFAULT_SERVER_IP "172.20.10.4"
// #define DEFAULT_SERVER_IP "192.168.1.6"
#define SERVER_PORT       3000

// Lấy địa chỉ server hiện tại (đọc từ Preferences, fallback về DEFAULT)
String getServerBase();

// Lưu IP mới vào Preferences
void setServerIP(String ip);

// Khởi tạo token trong Preferences nếu chưa có
void initSecrets();

#endif