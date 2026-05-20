#include "DHTesp.h"               // Thư viện cảm biến DHT22
#include <LiquidCrystal_I2C.h>    // Thư viện LCD I2C

// ================== KHAI BÁO CHÂN ==================

#define DHT_PIN 14     // Chân DATA của DHT22

#define LED 19         // Chân LED
#define PIR_PIN 13     // Chân PIR sensor

// ================== TẠO ĐỐI TƯỢNG ==================

DHTesp dhtSensor;                  // Đối tượng DHT22

LiquidCrystal_I2C LCD(0x27, 16, 2); // LCD I2C địa chỉ 0x27

// ================== SETUP ==================

void setup() {

  Serial.begin(115200);   // Khởi động Serial Monitor

  dhtSensor.setup(DHT_PIN, DHTesp::DHT22); // Khởi tạo DHT22

  pinMode(PIR_PIN, INPUT_PULLUP); // PIR là input

  pinMode(LED, OUTPUT);           // LED là output
  digitalWrite(LED, LOW);         // Tắt LED ban đầu

  LCD.init();         // Khởi tạo LCD
  LCD.backlight();    // Bật đèn nền LCD
}

// ================== LOOP ==================

void loop() {

  // ===== ĐỌC DỮ LIỆU DHT22 =====

  TempAndHumidity data = dhtSensor.getTempAndHumidity();

  // ===== HIỂN THỊ SERIAL =====

  Serial.println("Temp: " + String(data.temperature, 2) + " C");
  Serial.println("Humidity: " + String(data.humidity, 2) + " %");

  // ===== HIỂN THỊ LCD =====

  LCD.setCursor(0, 0);
  LCD.print("Temp:");
  LCD.print(data.temperature, 2);
  LCD.print((char)223);
  LCD.print("C");

  LCD.setCursor(0, 1);
  LCD.print("Hum:");
  LCD.print(data.humidity, 2);
  LCD.print("%");

  // ===== KIỂM TRA PIR SENSOR =====

  if (digitalRead(PIR_PIN) == HIGH) {

    digitalWrite(LED, HIGH); // Có chuyển động → bật LED

    Serial.println("Motion detected");
  }
  else {

    digitalWrite(LED, LOW); // Không có chuyển động → tắt LED

    Serial.println("Motion stop ....");
  }

  delay(2000); // Delay 2 giây
}