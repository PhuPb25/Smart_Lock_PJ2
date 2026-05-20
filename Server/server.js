const express = require('express');
const db      = require('./db');
const app     = express();

app.use(express.json({ limit: '2mb' }));

// =========================================
// POST /api/sync-user
// Body: { slot, name, code, sensor, fingerprint }
// FIX: dùng INSERT ... AS alias thay vì VALUES() (deprecated MySQL 8.0+)
// =========================================
// =========================================
// POST /api/sync-user
// =========================================
app.post('/api/sync-user', async (req, res) => {
    const { slot, name, code, sensor, fingerprint } = req.body;

    if (!name || !code) {
        return res.status(400).json({ error: 'Thiếu name hoặc code' });
    }

    try {
        // Lấy sensors hiện tại để merge (hỗ trợ cả AS608 + R558S cùng 1 code)
        const [existing] = await db.execute(
            `SELECT sensors FROM users WHERE code = ? LIMIT 1`, [code]
        );

        let currentSensors = existing.length > 0 && existing[0].sensors ? existing[0].sensors : '';
        let newSensors = currentSensors;

        if (sensor && !newSensors.includes(sensor)) {
            newSensors = newSensors ? `${newSensors},${sensor}` : sensor;
        }

        await db.execute(
            `INSERT INTO users (slot, name, code, sensors, fingerprint)
             VALUES (?, ?, ?, ?, ?)
             ON DUPLICATE KEY UPDATE
                slot        = COALESCE(VALUES(slot), slot),
                name        = VALUES(name),
                sensors     = ?,
                fingerprint = COALESCE(VALUES(fingerprint), fingerprint)`,
            [
                slot || null,
                name,
                code,
                newSensors,
                fingerprint || null,
                newSensors   // cho UPDATE
            ]
        );

        console.log(`[sync-user] code=${code} | ${name} | sensors=${newSensors} | template: ${fingerprint ? 'YES' : 'NO'}`);
        res.json({ status: 'ok', code, name, sensors: newSensors });

    } catch (err) {
        console.error('[sync-user] DB error:', err.message);
        res.status(500).json({ error: 'Lỗi database' });
    }
});

// =========================================
// GET /api/users?sensor=AS608
// =========================================
app.get('/api/users', async (req, res) => {
    const { sensor } = req.query;

    try {
        let query = `SELECT id, slot, name, code, sensors, fingerprint FROM users`;
        let params = [];

        if (sensor) {
            query += ` WHERE sensors LIKE ?`;
            params.push(`%${sensor}%`);
        }

        query += ` ORDER BY code ASC`;

        const [rows] = await db.execute(query, params);
        res.json(rows);
    } catch (err) {
        console.error('[users] DB error:', err.message);
        res.status(500).json({ error: 'Lỗi database' });
    }
});

// =========================================
// GET /api/template/:code
// =========================================
app.get('/api/template/:code', async (req, res) => {
    const code = req.params.code;

    try {
        const [rows] = await db.execute(
            `SELECT fingerprint FROM users WHERE code = ?`, [code]
        );
        if (rows.length === 0)      return res.status(404).json({ error: 'Không tìm thấy user' });
        if (!rows[0].fingerprint)   return res.status(404).json({ error: 'User chưa có template' });

        res.status(200).send(rows[0].fingerprint);
    } catch (err) {
        console.error('[template] DB error:', err.message);
        res.status(500).json({ error: 'Lỗi database' });
    }
});

// =========================================
// DELETE /api/user/:code
// =========================================
app.delete('/api/user/:code', async (req, res) => {
    const code = req.params.code;

    try {
        await db.execute(`DELETE FROM users WHERE code = ?`, [code]);
        console.log(`[delete] Đã xóa code=${code}`);
        res.status(200).json({ status: 'ok' });
    } catch (err) {
        console.error('[delete] DB error:', err.message);
        res.status(500).json({ error: 'Lỗi database' });
    }
});

// =========================================
// POST /api/log
// Body: { slot, uid, name, code, granted, sensor }
// sensor: "AS608" | "R558S" | "RFID" | "Remote"
// =========================================
app.post('/api/log', async (req, res) => {
    const { slot, uid, name, code, granted, sensor } = req.body;

    // Xác định method — ưu tiên sensor field
    let method = (sensor || 'fingerprint').toLowerCase();
    if (slot === 200 && !sensor) method = 'rfid';
    if (slot === 255 && !sensor) method = 'remote';

    try {
        await db.execute(
            `INSERT INTO access_logs (user_id, uid, name, code, method, granted)
             VALUES (?, ?, ?, ?, ?, ?)`,
            [
                (slot >= 1 && slot <= 127) ? slot : null,
                uid    || null,
                name   || 'Unknown',
                code   || null,
                method,
                granted ? 1 : 0
            ]
        );

        console.log(`[log] ${method} | code=${code} | ${name} | granted=${granted}`);
        res.status(200).json({ status: 'ok' });
    } catch (err) {
        console.error('[log] DB error:', err.message);
        res.status(500).json({ error: 'Lỗi database' });
    }
});

// =========================================
// GET /api/log?sensor=AS608|R558S|rfid|remote
// FIX: thêm filter "rfid" và "remote", sắp xếp mới nhất trước
// =========================================
app.get('/api/log', async (req, res) => {
    const { sensor } = req.query;

    try {
        let query  = `SELECT id, user_id, uid, name, code, method, granted,
                             DATE_FORMAT(created_at, '%H:%i:%s - %d/%m/%Y') AS time
                      FROM access_logs`;
        let params = [];

        if (sensor) {
            query  += ` WHERE method = ?`;
            params.push(sensor.toLowerCase());
        }

        query += ` ORDER BY created_at DESC LIMIT 100`;

        const [rows] = await db.execute(query, params);
        res.status(200).json(rows);
    } catch (err) {
        console.error('[log] DB error:', err.message);
        res.status(500).json({ error: 'Lỗi database' });
    }
});

// =========================================
// POST /api/rfid/sync
// Body: { uid, name }
// =========================================
app.post('/api/rfid/sync', async (req, res) => {
    const { uid, name } = req.body;

    if (!uid || !name)      return res.status(400).json({ error: 'Thiếu uid hoặc name' });
    if (uid.length !== 8)   return res.status(400).json({ error: 'UID phải 8 ký tự HEX' });

    try {
        await db.execute(
            `INSERT INTO rfid_cards (uid, name)
             VALUES (?, ?)
             ON DUPLICATE KEY UPDATE name = VALUES(name)`,
            [uid.toUpperCase(), name]
        );
        console.log(`[rfid-sync] UID: ${uid} | ${name}`);
        res.status(200).json({ status: 'ok' });
    } catch (err) {
        console.error('[rfid-sync] DB error:', err.message);
        res.status(500).json({ error: 'Lỗi database' });
    }
});

// =========================================
// DELETE /api/rfid/:uid
// =========================================
app.delete('/api/rfid/:uid', async (req, res) => {
    const uid = req.params.uid.toUpperCase();

    try {
        await db.execute(`DELETE FROM rfid_cards WHERE uid = ?`, [uid]);
        console.log(`[rfid-delete] UID: ${uid}`);
        res.status(200).json({ status: 'ok' });
    } catch (err) {
        console.error('[rfid-delete] DB error:', err.message);
        res.status(500).json({ error: 'Lỗi database' });
    }
});

// =========================================
// GET /api/rfid
// =========================================
app.get('/api/rfid', async (req, res) => {
    try {
        const [rows] = await db.execute(
            `SELECT uid, name FROM rfid_cards ORDER BY id`
        );
        res.status(200).json(rows);
    } catch (err) {
        console.error('[rfid] DB error:', err.message);
        res.status(500).json({ error: 'Lỗi database' });
    }
});

// =========================================
// Khởi động server
// =========================================
const PORT = 3000;
app.listen(PORT, '0.0.0.0', () => {
    console.log(`Smart Lock server chạy tại http://localhost:${PORT}`);
});