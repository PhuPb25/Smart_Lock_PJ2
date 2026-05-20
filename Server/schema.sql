-- =========================================
-- smart_lock — schema đầy đủ
-- Chạy trong phpMyAdmin hoặc MySQL CLI
-- =========================================

CREATE DATABASE IF NOT EXISTS smart_lock
  CHARACTER SET utf8mb4
  COLLATE utf8mb4_unicode_ci;

USE smart_lock;

-- =========================================
-- BẢNG USERS
-- code là định danh duy nhất (mã sinh viên/nhân viên...)
-- sensors: chuỗi "AS608" | "R558S" | "AS608,R558S"
-- fingerprint: base64 của template 512 bytes (AS608) hoặc 1024 bytes (R558S)
-- =========================================
CREATE TABLE IF NOT EXISTS users (
    id          INT            NOT NULL AUTO_INCREMENT,
    slot        TINYINT UNSIGNED,
    name        VARCHAR(100)   NOT NULL,
    code        VARCHAR(50)    NOT NULL,
    sensors     VARCHAR(50)    DEFAULT '',
    fingerprint MEDIUMTEXT,
    created_at  DATETIME       DEFAULT CURRENT_TIMESTAMP,
    updated_at  DATETIME       DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    PRIMARY KEY (id),
    UNIQUE KEY  uq_code (code)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- =========================================
-- BẢNG ACCESS_LOGS
-- method: 'as608' | 'r558s' | 'rfid' | 'remote'
-- granted: 1 = cho qua, 0 = từ chối
-- =========================================
CREATE TABLE IF NOT EXISTS access_logs (
    id         INT          NOT NULL AUTO_INCREMENT,
    user_id    INT,
    uid        VARCHAR(20),
    name       VARCHAR(100) DEFAULT 'Unknown',
    code       VARCHAR(50),
    method     ENUM('as608','r558s','rfid','remote','fingerprint') DEFAULT 'fingerprint',
    granted    TINYINT(1)   DEFAULT 0,
    created_at DATETIME     DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (id),
    INDEX idx_method    (method),
    INDEX idx_created   (created_at),
    INDEX idx_granted   (granted)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- =========================================
-- BẢNG RFID_CARDS
-- uid: 8 ký tự HEX viết hoa, VD: "D1F44453"
-- =========================================
CREATE TABLE IF NOT EXISTS rfid_cards (
    id         INT          NOT NULL AUTO_INCREMENT,
    uid        VARCHAR(8)   NOT NULL,
    name       VARCHAR(100) NOT NULL,
    created_at DATETIME     DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (id),
    UNIQUE KEY uq_uid (uid)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;