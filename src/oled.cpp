#include "oled.h"
#include <Wire.h>
#include <Arduino.h>
#include <config.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <time.h>

// =========================================
// KHỞI TẠO ĐỐI TƯỢNG OLED
// =========================================
static Adafruit_SSD1306 display(OLED_WIDTH, OLED_HEIGHT, &Wire, -1);
// Tham số -1: không dùng chân RESET phần cứng

// =========================================
// TRẠNG THÁI NỘI BỘ
// =========================================
static OledState  currentState  = OLED_IDLE;
static String     currentDetail = "";
static int        countAS1      = 0;
static int        countAS2      = 0;
static int        countRFID     = 0;
static unsigned long stateUntil = 0;  // 0 = không timeout (IDLE vĩnh viễn)

// Bao lâu mỗi trạng thái tự về IDLE (ms)
static const unsigned long TIMEOUT_GRANTED  = 3000;
static const unsigned long TIMEOUT_DENIED   = 3000;
static const unsigned long TIMEOUT_ENROLL   = 0;     // Chờ cho đến khi code gọi IDLE
static const unsigned long TIMEOUT_SYNCING  = 0;
static const unsigned long TIMEOUT_NO_WIFI  = 5000;

// =========================================
// HELPER — Vẽ đường kẻ ngang mỏng
// =========================================
static void drawDivider(int y) {
    display.drawFastHLine(0, y, OLED_WIDTH, SSD1306_WHITE);
}

// =========================================
// HELPER — Vẽ thanh tiêu đề (dòng trên cùng)
// =========================================
static void drawHeader(const char* title) {
    display.fillRect(0, 0, OLED_WIDTH, 12, SSD1306_WHITE);
    display.setTextColor(SSD1306_BLACK);
    display.setTextSize(1);
    display.setCursor(2, 2);
    display.print(title);
    display.setTextColor(SSD1306_WHITE);
}

// =========================================
// HELPER — In đồng hồ thực (góc phải header)
// =========================================
static void drawClock() {
    struct tm t;
    if (!getLocalTime(&t)) return;
    char buf[6];
    strftime(buf, sizeof(buf), "%H:%M", &t);

    display.setTextColor(SSD1306_BLACK);
    display.setTextSize(1);
    // Căn phải: mỗi ký tự rộng 6px, 5 ký tự = 30px
    display.setCursor(OLED_WIDTH - 32, 2);
    display.print(buf);
    display.setTextColor(SSD1306_WHITE);
}

// =========================================
// RENDER — IDLE: Giờ + thống kê user
// =========================================
static void renderIdle() {
    display.clearDisplay();

    // ----- Header -----
    drawHeader("  Smart Lock");
    drawClock();

    // ----- Đồng hồ lớn -----
    struct tm t;
    if (getLocalTime(&t)) {
        char buf[6];
        strftime(buf, sizeof(buf), "%H:%M", &t);
        display.setTextSize(2);
        display.setCursor(28, 16);
        display.print(buf);

        char dateBuf[12];
        strftime(dateBuf, sizeof(dateBuf), "%d/%m/%Y", &t);
        display.setTextSize(1);
        display.setCursor(24, 34);
        display.print(dateBuf);
    } else {
        display.setTextSize(2);
        display.setCursor(28, 16);
        display.print("--:--");
    }

    drawDivider(44);

    // ----- Thống kê 3 cột -----
    display.setTextSize(1);

    // AS1
    display.setCursor(0, 48);
    display.print("AS1:");
    display.setCursor(0, 57);
    display.printf("%3d", countAS1);

    // AS2 (giữa)
    display.setCursor(44, 48);
    display.print("AS2:");
    display.setCursor(44, 57);
    display.printf("%3d", countAS2);

    // RFID (phải)
    display.setCursor(90, 48);
    display.print("RFID:");
    display.setCursor(90, 57);
    display.printf("%3d", countRFID);

    display.display();
}

// =========================================
// RENDER — GRANTED: Mở khóa thành công
// =========================================
static void renderGranted() {
    display.clearDisplay();
    drawHeader("   TRUY CAP");

    // Icon check lớn
    display.setTextSize(3);
    display.setCursor(50, 18);
    display.print("OK");

    display.setTextSize(1);
    if (currentDetail.length() > 0) {
        // Giới hạn 21 ký tự/dòng (128px / 6px)
        display.setCursor(0, 50);
        String d = currentDetail;
        if (d.length() > 21) d = d.substring(0, 21);
        // Căn giữa
        int x = (OLED_WIDTH - (int)d.length() * 6) / 2;
        display.setCursor(max(0, x), 50);
        display.print(d);
    }

    // Vẽ viền trắng
    display.drawRect(0, 0, OLED_WIDTH, OLED_HEIGHT, SSD1306_WHITE);
    display.display();
}

// =========================================
// RENDER — DENIED: Từ chối truy cập
// =========================================
static void renderDenied() {
    display.clearDisplay();
    drawHeader("   TU CHOI");

    display.setTextSize(2);
    display.setCursor(36, 20);
    display.print("DENY");

    display.setTextSize(1);
    if (currentDetail.length() > 0) {
        String d = currentDetail;
        if (d.length() > 21) d = d.substring(0, 21);
        int x = (OLED_WIDTH - (int)d.length() * 6) / 2;
        display.setCursor(max(0, x), 50);
        display.print(d);
    }

    display.drawRect(0, 0, OLED_WIDTH, OLED_HEIGHT, SSD1306_WHITE);
    display.display();
}

// =========================================
// RENDER — SCANNING: Đang chờ vân tay / RFID
// =========================================
static void renderScanning() {
    display.clearDisplay();
    drawHeader(" DANG QUET");

    display.setTextSize(1);
    display.setCursor(8, 16);
    display.print("Dat ngon tay len");
    display.setCursor(8, 26);
    display.print("cam bien...");

    // Hiệu ứng dấu chấm động dựa theo millis
    int dots = (millis() / 400) % 4;
    display.setCursor(8, 42);
    for (int i = 0; i < dots; i++) display.print(".");

    if (currentDetail.length() > 0) {
        display.setCursor(0, 54);
        display.print(currentDetail.substring(0, 21));
    }

    display.display();
}

// =========================================
// RENDER — ENROLLING / ENROLL_STEP2
// =========================================
static void renderEnrolling(bool step2) {
    display.clearDisplay();
    drawHeader(" DANG KY VT");

    display.setTextSize(1);

    if (!step2) {
        display.setCursor(0, 16);
        display.print("Buoc 1/2: Dat ngon");
        display.setCursor(0, 26);
        display.print("tay len cam bien...");
    } else {
        display.setCursor(0, 16);
        display.print("Buoc 2/2: Nhac tay");
        display.setCursor(0, 26);
        display.print("roi dat lai...");
    }

    if (currentDetail.length() > 0) {
        drawDivider(44);
        display.setCursor(0, 48);
        display.print("Name: ");
        String d = currentDetail;
        if (d.length() > 15) d = d.substring(0, 15);
        display.print(d);
    }

    // Thanh tiến trình giả
    display.drawRect(0, 56, OLED_WIDTH, 7, SSD1306_WHITE);
    int progress = step2 ? OLED_WIDTH - 2 : (OLED_WIDTH - 2) / 2;
    display.fillRect(1, 57, progress, 5, SSD1306_WHITE);

    display.display();
}

// =========================================
// RENDER — ENROLL_OK
// =========================================
static void renderEnrollOk() {
    display.clearDisplay();
    drawHeader(" DANG KY XONG");

    display.setTextSize(2);
    display.setCursor(28, 18);
    display.print("DONE");

    display.setTextSize(1);
    if (currentDetail.length() > 0) {
        int x = (OLED_WIDTH - (int)min((int)currentDetail.length(), 21) * 6) / 2;
        display.setCursor(max(0, x), 52);
        display.print(currentDetail.substring(0, 21));
    }

    display.drawRect(0, 0, OLED_WIDTH, OLED_HEIGHT, SSD1306_WHITE);
    display.display();
}

// =========================================
// RENDER — ENROLL_FAIL
// =========================================
static void renderEnrollFail() {
    display.clearDisplay();
    drawHeader(" DANG KY LOI");

    display.setTextSize(1);
    display.setCursor(4, 20);
    display.print("Dang ky that bai!");
    display.setCursor(4, 34);
    display.print("Vui long thu lai.");

    if (currentDetail.length() > 0) {
        display.setCursor(0, 52);
        display.print(currentDetail.substring(0, 21));
    }

    display.display();
}

// =========================================
// RENDER — SYNCING
// =========================================
static void renderSyncing() {
    display.clearDisplay();
    drawHeader(" DONG BO DB");

    display.setTextSize(1);
    display.setCursor(4, 20);
    display.print("Dang tai du lieu");
    display.setCursor(4, 32);
    display.print("tu server...");

    // Spinner đơn giản dựa millis
    static const char spin[] = {'|', '/', '-', '\\'};
    char c = spin[(millis() / 200) % 4];
    display.setCursor(60, 44);
    display.print(c);

    if (currentDetail.length() > 0) {
        display.setCursor(0, 54);
        display.print(currentDetail.substring(0, 21));
    }

    display.display();
}

// =========================================
// RENDER — NO_WIFI
// =========================================
static void renderNoWifi() {
    display.clearDisplay();
    drawHeader("  KHONG WIFI");

    display.setTextSize(1);
    display.setCursor(4, 20);
    display.print("Khong co ket noi!");
    display.setCursor(4, 34);
    display.print("Hoat dong offline.");

    display.display();
}

// =========================================
// DISPATCH RENDER
// =========================================
static void renderCurrent() {
    switch (currentState) {
        case OLED_IDLE:         renderIdle();              break;
        case OLED_SCANNING:     renderScanning();          break;
        case OLED_GRANTED:      renderGranted();           break;
        case OLED_DENIED:       renderDenied();            break;
        case OLED_ENROLLING:    renderEnrolling(false);    break;
        case OLED_ENROLL_STEP2: renderEnrolling(true);     break;
        case OLED_ENROLL_OK:    renderEnrollOk();          break;
        case OLED_ENROLL_FAIL:  renderEnrollFail();        break;
        case OLED_SYNCING:      renderSyncing();           break;
        case OLED_NO_WIFI:      renderNoWifi();            break;
    }
}

// =========================================
// API CÔNG KHAI
// =========================================
void oledSetup() {
    Wire.begin(OLED_SDA_PIN, OLED_SCL_PIN);

    if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
        Serial.println("[OLED] Không tìm thấy màn hình — kiểm tra dây nối và địa chỉ I2C");
        return;
    }

    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(1);

    // Màn hình khởi động
    display.setCursor(14, 20);
    display.print("Smart Lock Pro");
    display.setCursor(4, 34);
    display.print("Dang khoi dong...");
    display.display();
    delay(1500);

    Serial.println("[OLED] Khoi tao OK");
}

void oledShow(OledState state, const String& detail) {
    currentState  = state;
    currentDetail = detail;

    // Đặt timeout tự về IDLE
    switch (state) {
        case OLED_GRANTED:    stateUntil = millis() + TIMEOUT_GRANTED;  break;
        case OLED_DENIED:     stateUntil = millis() + TIMEOUT_DENIED;   break;
        case OLED_NO_WIFI:    stateUntil = millis() + TIMEOUT_NO_WIFI;  break;
        case OLED_ENROLL_OK:  stateUntil = millis() + 3000;             break;
        case OLED_ENROLL_FAIL:stateUntil = millis() + 3000;             break;
        default:              stateUntil = 0;                            break;
    }

    renderCurrent();
}

void oledSetCounts(int as1, int as2, int rfid) {
    countAS1  = as1;
    countAS2  = as2;
    countRFID = rfid;
}

void oledLoop() {
    // Tự về IDLE sau timeout
    if (stateUntil > 0 && millis() > stateUntil) {
        stateUntil    = 0;
        currentState  = OLED_IDLE;
        currentDetail = "";
    }

    // IDLE và SCANNING cần re-render liên tục (cập nhật giờ / spinner)
    if (currentState == OLED_IDLE || currentState == OLED_SCANNING || currentState == OLED_SYNCING) {
        static unsigned long lastRefresh = 0;
        unsigned long interval = (currentState == OLED_IDLE) ? 1000 : 200;
        if (millis() - lastRefresh >= interval) {
            lastRefresh = millis();
            renderCurrent();
        }
    }
}