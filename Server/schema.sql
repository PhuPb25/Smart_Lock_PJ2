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
-- sensors: chuỗi "AS608" | "R503" | "AS608,R503"
-- fingerprint_as608: base64 template AS608 (512 bytes)
-- fingerprint_r503 : base64 template R503  (768 bytes)
-- =========================================
CREATE TABLE IF NOT EXISTS users (
    id                INT            NOT NULL AUTO_INCREMENT,
    slot              TINYINT UNSIGNED,
    name              VARCHAR(100)   NOT NULL,
    code              VARCHAR(50)    NOT NULL,
    sensors           VARCHAR(50)    DEFAULT '',
    fingerprint_as608 MEDIUMTEXT,
    fingerprint_r503  MEDIUMTEXT,
    created_at        DATETIME       DEFAULT CURRENT_TIMESTAMP,
    updated_at        DATETIME       DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    PRIMARY KEY (id),
    UNIQUE KEY uq_code (code)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- =========================================
-- Script ALTER nếu đã có bảng cũ (chạy 1 lần)
-- =========================================
-- ALTER TABLE users
--   CHANGE COLUMN fingerprint fingerprint_as608 MEDIUMTEXT,
--   ADD COLUMN fingerprint_r503 MEDIUMTEXT AFTER fingerprint_as608;
-- UPDATE users SET fingerprint_as608 = fingerprint WHERE fingerprint IS NOT NULL;

-- =========================================
-- BẢNG ACCESS_LOGS
-- method: 'as608' | 'r503' | 'rfid' | 'remote'
-- granted: 1 = cho qua, 0 = từ chối
-- =========================================
CREATE TABLE IF NOT EXISTS access_logs (
    id         INT          NOT NULL AUTO_INCREMENT,
    user_id    INT,
    uid        VARCHAR(20),
    name       VARCHAR(100) DEFAULT 'Unknown',
    code       VARCHAR(50),
    method     ENUM('as608','r503','rfid','remote','fingerprint') DEFAULT 'fingerprint',
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