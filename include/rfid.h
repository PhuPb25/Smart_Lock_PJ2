#ifndef RFID_H
#define RFID_H

#include <Arduino.h>
//Lưu RFIDUser
#define MAX_RFID 20

struct RFIDUser {
  byte uid[4];
  String uidStr;
  String name;
};
extern RFIDUser rfidUsers[MAX_RFID];
extern int rfidCount;

void loadRFID();
void saveRFID(String uid, String name);
void checkRFID();

#endif