const express = require('express');
const db      = require('./db');
const app     = express();

app.use(express.json({ limit: '50mb' }));
app.use(express.urlencoded({ extended: true, limit: '50mb' }));

// =========================================
// POST /api/sync-user
// Body: { slot, name, code, sensor, fingerprint }
// sensor: "AS608" | "R503"
// fingerprint: base64 template tương ứng với sensor
// =========================================
app.post('/api/sync-user', async (req, res) => {
    const { slot, name, code, sensor, fingerprint } = req.body;

    if (!name || !code) return res.status(400).json({ error: 'Thiếu name hoặc code' });

    try {
        // --- Cập nhật sensors string ---
        let newSensors = sensor || '';
        if (sensor) {
            const [exist] = await db.execute(`SELECT sensors FROM users WHERE code = ?`, [code]);
            let curr = exist.length > 0 ? (exist[0].sensors || '') : '';
            if (!curr.includes(sensor)) {
                newSensors = curr ? `${curr},${sensor}` : sensor;
            } else {
                newSensors = curr; // giữ nguyên nếu đã có
            }
        }

        // --- Xác định cột fingerprint cần cập nhật ---
        // COALESCE(VALUES(col), col) đảm bảo chỉ ghi đè cột đúng sensor,
        // cột còn lại giữ nguyên giá trị cũ.
        const fpAs608 = (sensor === 'AS608' && fingerprint) ? fingerprint : null;
        const fpR503  = (sensor === 'R503'  && fingerprint) ? fingerprint : null;

        await db.execute(
            `INSERT INTO users (slot, name, code, sensors, fingerprint_as608, fingerprint_r503)
             VALUES (?, ?, ?, ?, ?, ?)
             ON DUPLICATE KEY UPDATE
                name              = VALUES(name),
                sensors           = VALUES(sensors),
                fingerprint_as608 = COALESCE(VALUES(fingerprint_as608), fingerprint_as608),
                fingerprint_r503  = COALESCE(VALUES(fingerprint_r503),  fingerprint_r503)`,
            [slot || null, name, code, newSensors, fpAs608, fpR503]
        );

        console.log(`[sync-user] OK - code=${code} | sensor=${sensor} | fp_len=${fingerprint ? fingerprint.length : 0}`);
        res.status(200).json({ status: 'ok' });

    } catch (err) {
        console.error('[sync-user] ERROR:', err.message);
        res.status(500).json({ error: err.message });
    }
});

// =========================================
// GET /api/users?sensor=AS608|R503
// Trả về đúng cột fingerprint theo sensor được yêu cầu
// =========================================
app.get('/api/users', async (req, res) => {
    const { sensor } = req.query;

    try {
        // Chọn cột fingerprint phù hợp với sensor
        let fpCol = 'NULL AS fingerprint';
        if (sensor === 'AS608') fpCol = 'fingerprint_as608 AS fingerprint';
        if (sensor === 'R503')  fpCol = 'fingerprint_r503  AS fingerprint';

        let query = `SELECT id, slot, name, code, sensors, ${fpCol} FROM users`;
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
// GET /api/users/all
// Trả về cả 2 template (dùng cho trang quản lý)
// =========================================
app.get('/api/users/all', async (req, res) => {
    try {
        const [rows] = await db.execute(
            `SELECT id, slot, name, code, sensors,
                    fingerprint_as608, fingerprint_r503,
                    created_at, updated_at
             FROM users ORDER BY code ASC`
        );
        res.json(rows);
    } catch (err) {
        console.error('[users/all] DB error:', err.message);
        res.status(500).json({ error: 'Lỗi database' });
    }
});

// =========================================
// GET /api/template/:code?sensor=AS608|R503
// =========================================
app.get('/api/template/:code', async (req, res) => {
    const code   = req.params.code;
    const sensor = req.query.sensor || 'AS608';

    const col = sensor === 'R503' ? 'fingerprint_r503' : 'fingerprint_as608';

    try {
        const [rows] = await db.execute(
            `SELECT ${col} AS fingerprint FROM users WHERE code = ?`, [code]
        );
        if (rows.length === 0)       return res.status(404).json({ error: 'Không tìm thấy user' });
        if (!rows[0].fingerprint)    return res.status(404).json({ error: `User chưa có template ${sensor}` });

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
// DELETE /api/user/:code/sensor
// Xóa template của 1 sensor, giữ sensor còn lại
// Body: { sensor: "AS608" | "R503" }
// =========================================
app.delete('/api/user/:code/sensor', async (req, res) => {
    const code   = req.params.code;
    const sensor = req.body.sensor;

    if (!sensor) return res.status(400).json({ error: 'Thiếu sensor' });

    const col = sensor === 'R503' ? 'fingerprint_r503' : 'fingerprint_as608';

    try {
        // Xóa template và gỡ tên sensor khỏi sensors string
        const [rows] = await db.execute(`SELECT sensors FROM users WHERE code = ?`, [code]);
        if (rows.length === 0) return res.status(404).json({ error: 'Không tìm thấy user' });

        let sensors = (rows[0].sensors || '').split(',').filter(s => s !== sensor).join(',');

        await db.execute(
            `UPDATE users SET ${col} = NULL, sensors = ? WHERE code = ?`,
            [sensors, code]
        );
        console.log(`[delete-sensor] code=${code} | sensor=${sensor}`);
        res.status(200).json({ status: 'ok' });
    } catch (err) {
        console.error('[delete-sensor] DB error:', err.message);
        res.status(500).json({ error: 'Lỗi database' });
    }
});

// =========================================
// POST /api/log
// =========================================
app.post('/api/log', async (req, res) => {
    const { slot, uid, name, code, granted, sensor } = req.body;

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
// GET /api/log?sensor=AS608|R503|rfid|remote
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