#include <Arduino.h>
#include <time.h>

#ifndef LOG_H
#define LOG_H

// =========================================
// AS608
// =========================================
void logAccessAS1(int id, const String& uid, const String& name, bool granted, const String& code);
void logAccessAS2(int id, const String& uid, const String& name, bool granted, const String& code);;

#endif