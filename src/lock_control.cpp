#include "lock_control.h"
#include "globals.h"
#include "config.h"

void openLock() {
    digitalWrite(RELAY_PIN, HIGH);
    pixels.setPixelColor(0, pixels.Color(0, 255, 0));
    pixels.show();
    delay(LOCK_OPEN_DURATION_MS);
    digitalWrite(RELAY_PIN, LOW);
    pixels.clear();
    pixels.show();
}

void denyAccess() {
    pixels.setPixelColor(0, pixels.Color(255, 0, 0));
    pixels.show();
    delay(1000);
    pixels.clear();
    pixels.show();
}