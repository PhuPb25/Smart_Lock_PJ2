#include "r503_driver.h"

R503::R503(HardwareSerial& serial) : _serial(serial) {}

bool R503::begin(uint32_t baud, int rxPin, int txPin) {
    if (rxPin >= 0 && txPin >= 0) {
        _serial.begin(baud, SERIAL_8N1, rxPin, txPin); // Khoi tao Serial 8N1 voi cac chan tùy chinh
    } else {
        _serial.begin(baud, SERIAL_8N1);               // Khoi tao Serial 8N1 voi cac chan mac dinh
    }
    delay(1500); // Cho module hoat dong on dinh sau khi khoi dong

    while (_serial.available()) _serial.read();        // Xa toan bo du lieu rac trong buffer

    for (int attempt = 1; attempt <= 3; attempt++) {   // Thu handshake toi da 3 lan voi lenh 0x40
        Serial.printf("[R503] Handshake lan %d...\n", attempt);
        sendCommand(0x40, nullptr, 0);

        uint8_t buf[32];
        if (readPacket(buf, 3000) && buf[9] == 0x00) {
            Serial.println("[R503] Khoi tao thanh cong");
            return true;
        }

        Serial.printf("[R503] Handshake that bai (code=0x%02X), thu lai...\n", lastError);
        delay(500);
        while (_serial.available()) _serial.read();    // Xoa buffer truoc khi thu lai lan nua
    }

    Serial.println("[R503] Handshake that bai sau 3 lan — kiem tra day noi");
    return false;
}

// ==================== CORE PROTOCOL ====================

bool R503::sendCommand(uint8_t cmdCode, const uint8_t* params, uint16_t paramLen) {
    uint8_t packet[128] = {0}; // Tong packet = header(9) + params + checksum(2)
    uint16_t idx = 0;

    packet[idx++] = 0xEF; packet[idx++] = 0x01;        // Header: Start of packet
    packet[idx++] = 0xFF; packet[idx++] = 0xFF;        // Dia chi module (mac dinh 0xFFFFFFFF)
    packet[idx++] = 0xFF; packet[idx++] = 0xFF;
    packet[idx++] = 0x01;                              // PID: Gói lenh (Command packet)

    uint16_t length = paramLen + 3;                    // length = cmdCode(1) + params + checksum(2)
    packet[idx++] = length >> 8;                       // Chieu dai (High byte)
    packet[idx++] = length & 0xFF;                     // Chieu dai (Low byte)

    packet[idx++] = cmdCode;                           // Ma lenh dieu khien
    if (params && paramLen > 0) {
        memcpy(&packet[idx], params, paramLen);        // Copy cac tham so vao packet
        idx += paramLen;
    }

    uint16_t sum = 0;                                  // Checksum tinh tu byte PID(6) den het params
    for (uint16_t i = 6; i < idx; i++) sum += packet[i];
    packet[idx++] = sum >> 8;                          // Checksum (High byte)
    packet[idx++] = sum & 0xFF;                        // Checksum (Low byte)

    while (_serial.available()) _serial.read();        // Xoa buffer truoc khi gui lenh
    _serial.write(packet, idx);                        // Gui toan bo packet xuong module
    return true;
}

bool R503::readPacket(uint8_t* buf, uint32_t timeout) {
    uint32_t start = millis();
    int idx = 0;

    while (millis() - start < timeout) {
        while (_serial.available()) {
            uint8_t c = _serial.read();

            if (idx == 0 && c != 0xEF) continue;       // Tim byte bat dau 0xEF
            
            if (idx == 1) {
                if (c != 0x01) {                       // Kiem tra byte thu 2 la 0x01
                    idx = 0;
                    continue;
                }
            }

            buf[idx++] = c;                            // Luu du lieu vao buffer

            if (idx >= 9) {                            // Khi da nhan du phan Header
                uint16_t len = ((uint16_t)buf[7] << 8) | buf[8];
                uint16_t totalExpected = len + 9;      // Tong so byte can nhan = header(9) + len

                if (idx >= totalExpected) {            // Da nhan du toan bo packet
                    if (buf[6] == 0x07) lastError = buf[9]; // Luu ma loi neu day la goi tra loi (ACK)
                    return true;
                }
            }
        }
        delay(1); // Nghi 1ms tranh qua tai CPU
    }
    
    lastError = 0xFF;                                  // Timeout
    return false;
}

// ==================== TEMPLATE FUNCTIONS ====================

bool R503::downloadTemplate(uint16_t id, uint8_t* buffer, uint16_t* outLength) {
    *outLength = 0;
    uint8_t buf[1024];

    Serial.printf("[R503] Download template ID=%d...\n", id);

    uint8_t loadParam[3] = {0x01, (uint8_t)(id >> 8), (uint8_t)(id & 0xFF)}; // 1. Lenh LoadChar
    sendCommand(0x07, loadParam, 3);
    if (!readPacket(buf, 3000) || buf[9] != 0x00) {
        Serial.println("[R503] LoadChar failed");
        return false;
    }

    uint8_t upParam[1] = {0x01};                                             // 2. Lenh UpChar
    sendCommand(0x08, upParam, 1);

    if (!readPacket(buf, 3000) || buf[6] != 0x07 || buf[9] != 0x00) {        // Doc va hung goi ACK(0x07) cua UpChar
        Serial.println("[R503] UpChar ACK failed");
        return false;
    }

    uint16_t idx = 0;                                                        // 3. Bat dau doc cac goi Data tho
    while (true) {
        if (!readPacket(buf, 4000)) {
            Serial.println("[R503] UpChar data packet timeout");
            break;
        }

        uint8_t pid = buf[6];
        if (pid != 0x02 && pid != 0x08) continue;                            // Chi xu ly Data Packet(0x02) va End Packet(0x08)

        uint16_t pktLen = ((uint16_t)buf[7] << 8) | buf[8];
        uint16_t dataLen = pktLen - 2;

        for (uint16_t i = 0; i < dataLen && idx < 768; i++) {                // Doc data vao buffer dich
            buffer[idx++] = buf[9 + i];
        }

        if (pid == 0x08) break;                                              // Ket thuc luong du lieu (End of data)
    }

    *outLength = idx;
    Serial.printf("[R503] Downloaded: %d bytes\n", idx);
    return (idx == 768);                                                     // Kiem tra du lieu phai du 768 bytes
}

bool R503::readDataPackets(uint8_t* buffer, uint16_t maxLen, uint16_t* receivedLen) {
    uint8_t pkt[800];
    uint16_t total = 0;

    while (total < maxLen) {
        if (!readPacket(pkt, 3000)) {
            Serial.println("[R503] readDataPackets: timeout");
            break;
        }

        uint8_t pid = pkt[6];
        if (pid != 0x02 && pid != 0x08) {                                    // Bo qua cac goi khong phai la data
            Serial.printf("[R503] readDataPackets: unexpected pid=0x%02X\n", pid);
            break;
        }

        uint16_t pktLen = ((uint16_t)pkt[7] << 8) | pkt[8];
        uint16_t dataLen = pktLen - 2;                                       // Loai bo 2 byte checksum

        for (uint16_t i = 0; i < dataLen && total < maxLen; i++) {           // Data thuc te bat dau tu byte so 9
            buffer[total++] = pkt[9 + i];
        }

        if (pid == 0x08) break;                                              // End-of-data packet
    }

    *receivedLen = total;
    return total > 200;
}

bool R503::uploadTemplate(uint16_t id, const uint8_t* templateData, uint16_t len) {
    Serial.printf("[R503] Uploading template to ID=%d (%d bytes)...\n", id, len);

    // 1. DownChar (0x09) - 1 byte tham số BufferID
    uint8_t params[1] = {0x01}; 
    sendCommand(0x09, params, 1);

    uint8_t resp[32]; // Mảng 32 bytes an toàn, không bị Stack Smashing
    if (!readPacket(resp, 2000) || resp[9] != 0x00) {
        Serial.printf("[R503] DownChar thất bại, ACK: 0x%02X\n", resp[9]);
        return false;
    }
    delay(50);  

    // 2. Ép xuống cấu hình an toàn nhất: Gói 64 bytes
    const uint16_t PKT_SIZE = 64;
    int totalPkts = len / PKT_SIZE; // 768 / 64 = 12 gói

    for (int i = 0; i < totalPkts; i++) {
        uint8_t pid = (i == totalPkts - 1) ? 0x08 : 0x02;
        uint8_t pkt[80] = {0}; 
        int idx = 0;

        pkt[idx++] = 0xEF; pkt[idx++] = 0x01;
        pkt[idx++] = 0xFF; pkt[idx++] = 0xFF; 
        pkt[idx++] = 0xFF; pkt[idx++] = 0xFF;
        pkt[idx++] = pid;

        uint16_t plen = PKT_SIZE + 2; 
        pkt[idx++] = plen >> 8;
        pkt[idx++] = plen & 0xFF;

        uint16_t sum = pid + (plen >> 8) + (plen & 0xFF);
        for (uint16_t j = 0; j < PKT_SIZE; j++) {
            uint8_t b = templateData[i * PKT_SIZE + j];
            pkt[idx++] = b;
            sum += b;
        }
        pkt[idx++] = sum >> 8;
        pkt[idx++] = sum & 0xFF;

        _serial.write(pkt, idx);
        delay(30); 

        // Lọc bỏ mọi byte rác cảm biến văng ra nếu quá trình nạp bị nghẽn
        while (_serial.available()) {
            uint8_t err = _serial.read();
            Serial.printf("[R503] Cảnh báo, module nhả byte rác trong lúc nạp: 0x%02X\n", err);
        }
    }

    delay(150);

    // 3. Lệnh StoreChar (0x06)
    uint8_t storeParams[3] = {0x01, (uint8_t)(id >> 8), (uint8_t)(id & 0xFF)};
    sendCommand(0x06, storeParams, 3);

    if (readPacket(resp, 3000) && resp[9] == 0x00) {
        Serial.println("[R503] StoreChar THÀNH CÔNG!");
        return true;
    } 

    Serial.printf("[R503] StoreChar failed: 0x%02X\n", resp[9]);
    return false;
}

// ==================== CÁC HÀM KHÁC ====================

bool R503::isTouched() {
    sendCommand(0x01, nullptr, 0);                                     // GetImage(0x01): Kiem tra nhanh ngón tay
    uint8_t buf[32];
    if (!readPacket(buf, 300)) return false;                           // Timeout ngan (300ms) de tranh delay he thong
    return buf[9] == 0x00;                                             // 0x00 = co ngón tay, 0x02 = khong co ngón tay
}

bool R503::enroll(uint16_t pageID) {
    uint8_t buf[64];
    Serial.printf("[R503] Dang dang ky Slot %d...\n", pageID);

    Serial.println("[R503] Moi dat ngon tay...");                      // --- LAN QUET 1 ---
    setLED(0x01, 0x02);                                                // LED Tho cham Xanh duong (Cho ngon tay)

    while (true) {
        sendCommand(0x01);
        if (!readPacket(buf, 2000)) continue;
        if (buf[9] == 0x00) break;                                     // 0x00: Da thay van tay
        delay(200);
    }

    setLED(0x02, 0x03);                                                // LED Nhay Tim (Dang doc va tao mau)
    uint8_t p1[1] = {0x01};
    sendCommand(0x02, p1, 1);                                          // Chuyen anh thanh dac trung luu o Buffer 1

    if (!readPacket(buf, 3000) || buf[9] != 0x00) {
        Serial.println("[R503] Loi doc mau lan 1");
        setLED(0x02, 0x01);                                            // LED Nhay Do (Loi)
        delay(2000);
        setLED(0x04, 0x00);
        return false;
    }

    Serial.println("[R503] Vui long nhac ngon tay ra...");             // --- YEU CAU NHAC NGON TAY ---
    setLED(0x03, 0x03);                                                // LED Sang dung Tim (Cho nhac tay)
    delay(1000);

    while (true) {
        sendCommand(0x01);
        if (!readPacket(buf, 1000)) continue;
        if (buf[9] == 0x02) break;                                     // 0x02: Da nhac ngon tay ra hoan toan
        delay(100);
    }

    Serial.println("[R503] Moi dat LAI ngon tay cu...");               // --- LAN QUET 2 ---
    setLED(0x01, 0x02);                                                // LED Tho cham Xanh duong (Cho ngon tay lan 2)

    while (true) {
        sendCommand(0x01);
        if (!readPacket(buf, 2000)) continue;
        if (buf[9] == 0x00) break;
        delay(200);
    }

    setLED(0x02, 0x03);                                                // LED Nhay Tim (Dang doc mau 2)
    uint8_t p2[1] = {0x02};
    sendCommand(0x02, p2, 1);                                          // Chuyen anh thanh dac trung luu o Buffer 2

    if (!readPacket(buf, 3000) || buf[9] != 0x00) {
        Serial.println("[R503] Loi doc mau lan 2");
        setLED(0x02, 0x01);                                            // LED Nhay Do (Loi)
        delay(2000);
        setLED(0x04, 0x00);
        return false;
    }

    sendCommand(0x05);                                                 // --- GOP MAU VA LUU --- (Lenh RegModel)
    if (!readPacket(buf, 3000) || buf[9] != 0x00) {
        Serial.println("[R503] Hai lan quet khong khop nhau!");
        setLED(0x02, 0x01);
        delay(2000);
        setLED(0x04, 0x00);
        return false;
    }

    uint8_t storeParam[3] = {0x01, (uint8_t)(pageID >> 8), (uint8_t)(pageID & 0xFF)};
    sendCommand(0x06, storeParam, 3);                                  // Lenh StoreChar ghi vao flash
    
    if (!readPacket(buf, 3000) || buf[9] != 0x00) {
        Serial.println("[R503] Loi ghi vao bo nho Flash");
        setLED(0x02, 0x01);
        delay(2000);
        setLED(0x04, 0x00);
        return false;
    }

    Serial.printf("[R503] Enroll OK - slot=%d\n", pageID);
    setLED(0x03, 0x02);                                                // LED Sang ruc ro Xanh duong (Thanh cong)
    delay(2000);        
    setLED(0x01, 0x07);                                                // Tat den de tiet kiem nang luong
    return true;
}

bool R503::verify(uint16_t* matchedID, uint16_t* score) {
    uint8_t buf[64];
    while (_serial.available()) _serial.read();                        // Xoa buffer truoc khi verify

    sendCommand(0x01);                                                 // STEP 1: Lệnh GetImage
    if (!readPacket(buf, 3000)) {
        Serial.println("[R503] GetImage timeout");
        return false;
    }

    if (buf[9] != 0x00) {
        if (buf[9] != 0x02) {
            Serial.printf("[R503] GetImage failed: 0x%02X\n", buf[9]);
        }
        return false;
    }

    uint8_t img2tz[1] = {0x01};                                        // STEP 2: Lệnh Img2Tz (Buffer 1)
    sendCommand(0x02, img2tz, 1);
    
    if (!readPacket(buf, 3000)) {
        Serial.println("[R503] Img2Tz timeout");
        return false;
    }
    if (buf[9] != 0x00) {
        Serial.printf("[R503] Img2Tz failed: 0x%02X\n", buf[9]);
        return false;
    }

    uint8_t searchParam[5] = {0x01, 0x00, 0x00, 0x00, 0xC8};           // STEP 3: Lệnh Search (Tim kiem trong toan bo 200 slots)
    sendCommand(0x04, searchParam, 5);

    if (!readPacket(buf, 3000)) {
        Serial.println("[R503] Search timeout");
        return false;
    }

    uint8_t conf = buf[9];
    if (conf != 0x00) {
        if (conf == 0x09) Serial.println("[R503] No match");           // Ma 0x09: Khong tim thay van tay khop
        else Serial.printf("[R503] Search failed: 0x%02X\n", conf);
        return false;
    }

    uint16_t id = ((uint16_t)buf[10] << 8) | buf[11];                  // Giai ma Slot ID 
    uint16_t sc = ((uint16_t)buf[12] << 8) | buf[13];                  // Giai ma Diem tin cay (Score)

    Serial.printf("[R503] MATCH id=%d score=%d\n", id, sc);
    if (matchedID) *matchedID = id;
    if (score) *score = sc;

    return true;
}

bool R503::emptyDatabase() {
    sendCommand(0x0D, nullptr, 0);                                     // Lenh xoa toan bo van tay (0x0D)
    uint8_t buf[32];
    return readPacket(buf) && buf[9] == 0x00;
}

bool R503::getTemplateCount(uint16_t* count) {
    sendCommand(0x1D, nullptr, 0);                                     // Lenh dem so luong mau (0x1D)
    uint8_t buf[32];
    if (readPacket(buf) && buf[9] == 0x00) {
        *count = (buf[10] << 8) | buf[11];                             // So luong hien tai tren cam bien
        return true;
    }
    return false;
}

bool R503::setLED(uint8_t ctrl, uint8_t color) {
    uint8_t params[4] = {ctrl, 0x60, color, 0x00};                     // Lenh dieu khien LED 0x35: ctrl, speed, color, times
    sendCommand(0x35, params, 4);
    delay(50);
    return true;
}

bool R503::deleteTemplate(uint16_t id) {
    Serial.printf("[R503] Deleting template ID=%d...\n", id);
    uint8_t params[4] = {(uint8_t)(id >> 8), (uint8_t)(id & 0xFF), 0x00, 0x01}; // Lenh DeleteChar(0x0C): PageID + Count
    sendCommand(0x0C, params, 4);
    
    uint8_t buf[32];
    if (readPacket(buf, 2000) && buf[9] == 0x00) {
        Serial.printf("[R503] Da xoa template slot %d\n", id);
        return true;
    }
    
    Serial.printf("[R503] Xoa template that bai (code 0x%02X)\n", buf[9]);
    return false;
}

// ====================================================================
// LỆNH LƯU TEMPLATE TỪ BUFFER VÀO FLASH CẢM BIẾN (MÃ LỆNH 0x06)
// ====================================================================
uint8_t R503::storeModel(uint16_t id, uint8_t slot) {
    // Tham số gồm: Số thứ tự Buffer (slot), ID High Byte, ID Low Byte
    uint8_t storeParam[3] = {slot, (uint8_t)(id >> 8), (uint8_t)(id & 0xFF)};
    if (!sendCommand(0x06, storeParam, 3)) return 0xFF; // Lỗi gửi lệnh
    
    uint8_t buf[32];
    if (readPacket(buf, 3000)) {
        return buf[9]; // Trả về mã phản hồi từ cảm biến (0x00 là thành công)
    }
    return 0xFF; // Lỗi timeout hoặc phản hồi không hợp lệ
}

String R503::getErrorMessage(uint8_t code) const {
    switch(code) {
        case 0x00: return "OK";
        case 0x09: return "No match";
        case 0x27: return "ID already exists";
        default:   return "Error 0x" + String(code, HEX);
    }
}