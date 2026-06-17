// =========================================
// secrets.h 
// =========================================
#ifndef SECRETS_H
#define SECRETS_H

// Đổi WiFi
#define WIFI_SSID     "DangDat"
#define WIFI_PASSWORD "15042005"

// #define WIFI_SSID     "Pb (2)"
// #define WIFI_PASSWORD "25110110"

// Token mở khóa từ xa — đặt chuỗi ngẫu nhiên dài ≥ 16 ký tự
#define UNLOCK_TOKEN  "2511200501102005"

// API key đơn giản để bảo vệ các endpoint quản lý trên ESP32
// Client phải gửi header: X-API-Key: <giá trị này>
#define API_KEY       "2511"

#endif