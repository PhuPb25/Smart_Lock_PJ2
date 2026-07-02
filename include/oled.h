#pragma once
#include <Arduino.h>

// =========================================
// CÁC TRẠNG THÁI HIỂN THỊ
// =========================================
enum OledState {
    OLED_IDLE,          // Chờ — hiện giờ + số user
    OLED_SCANNING,      // Đang chờ vân tay / thẻ RFID
    OLED_GRANTED,       // Mở khóa thành công
    OLED_DENIED,        // Từ chối
    OLED_ENROLLING,     // Đang đăng ký vân tay
    OLED_ENROLL_STEP2,  // Yêu cầu quét lần 2
    OLED_ENROLL_OK,     // Đăng ký hoàn tất
    OLED_ENROLL_FAIL,   // Đăng ký thất bại
    OLED_SYNCING,       // Đang đồng bộ với server
    OLED_NO_WIFI,       // Mất kết nối WiFi
};

// =========================================
// API CÔNG KHAI
// =========================================
void oledSetup();

void oledLoop();   // Gọi trong loop() — xử lý timeout tự động về IDLE

// Đặt trạng thái + dòng phụ tuỳ chọn (tên/code/slot...)
void oledShow(OledState state, const String& detail = "");

// Cập nhật số liệu ở màn hình IDLE
void oledSetCounts(int as1, int as2, int rfid);