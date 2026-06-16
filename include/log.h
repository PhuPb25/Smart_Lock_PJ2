#include <Arduino.h>
#include <time.h>

#ifndef LOG_H
#define LOG_H

struct AccessLog {
  int id;
  String uid;
  String name;
  bool granted;
  time_t timestamp;
};

// =========================================
// AS608 — log riêng
// =========================================
extern AccessLog logAS[20];
extern int logIndexAS;

// id    = slot (1–127) hoặc 255 (remote)
// uid   = code của user vân tay (hoặc "" nếu không khớp)
// name  = tên user
// granted = true/false
// code  = mã số định danh (dùng để đẩy lên DB)
void logAccessAS(int id, const String& uid, const String& name,
                 bool granted, const String& code = "");

// =========================================
// R503 — log riêng
// =========================================
extern AccessLog logRS[20];
extern int logIndexRS;

void logAccessRS(int id, const String& uid, const String& name,
                 bool granted, const String& code = "");

#endif