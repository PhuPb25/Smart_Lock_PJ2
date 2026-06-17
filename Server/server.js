const express = require('express');
const db      = require('./db');
const app     = express();

app.use(express.json({ limit: '50mb' }));
app.use(express.urlencoded({ extended: true, limit: '50mb' }));

// =========================================
// HELPER: Normalize sensor name
// =========================================
function normalizeSensor(sensor) {
    if (!sensor) return null;
    // Chuyển chữ hoa và hỗ trợ chuẩn hóa cả ký tự gạch ngang '-' thành gạch dưới '_'
    const s = sensor.toUpperCase().replace('-', '_');
    if (s === 'AS608_1' || s === 'AS608_2' || s === 'AS608') return 'AS608';
    if (s === 'R503') return 'R503';
    return s;
}

// =========================================
// POST /api/sync-user
// Hỗ trợ cảm biến: AS608_1, AS608_2, AS608, R503
// =========================================
app.post('/api/sync-user', async (req, res) => {
    let { slot, name, code, sensor, fingerprint } = req.body;

    if (!name || !code) return res.status(400).json({ error: 'Thiếu name hoặc code' });

    const normSensor = normalizeSensor(sensor);
    console.log(`[sync-user] Original sensor: ${sensor} → Normalized: ${normSensor}`);

    try {
        // Kiểm tra user đã tồn tại chưa để thực hiện gộp (merge) danh sách cảm biến, tránh mất dữ liệu cũ
        const [exist] = await db.execute(`SELECT slot, sensors FROM users WHERE code = ?`, [code]);
        let currentSensors = exist.length > 0 ? (exist[0].sensors || '') : '';
        let currentSlot = exist.length > 0 ? exist[0].slot : null;
        
        let newSensors = currentSensors;
        if (normSensor) {
            const sensorArray = currentSensors ? currentSensors.split(',') : [];
            if (!sensorArray.includes(normSensor)) {
                sensorArray.push(normSensor);
            }
            newSensors = sensorArray.join(',');
        }

        // Xác định cột fingerprint tương ứng dựa trên loại cảm biến được gửi lên
        const fpAs608 = (normSensor === 'AS608' && fingerprint) ? fingerprint : null;
        const fpR503  = (normSensor === 'R503'  && fingerprint) ? fingerprint : null;

        // Ưu tiên slot mới, nếu không truyền slot (ví dụ từ Web Admin) thì giữ nguyên slot cũ
        const finalSlot = slot !== undefined ? slot : currentSlot;

        await db.execute(
            `INSERT INTO users (slot, name, code, sensors, fingerprint_as608, fingerprint_r503)
             VALUES (?, ?, ?, ?, ?, ?)
             ON DUPLICATE KEY UPDATE
                name              = VALUES(name),
                slot              = IFNULL(VALUES(slot), slot),
                sensors           = VALUES(sensors),
                fingerprint_as608 = COALESCE(VALUES(fingerprint_as608), fingerprint_as608),
                fingerprint_r503  = COALESCE(VALUES(fingerprint_r503),  fingerprint_r503)`,
            [finalSlot, name, code, newSensors, fpAs608, fpR503]
        );

        console.log(`[sync-user] OK - code=${code} | sensor=${sensor} | fp_len=${fingerprint ? fingerprint.length : 0}`);
        res.status(200).json({ status: 'ok' });

    } catch (err) {
        console.error('[sync-user] ERROR:', err.message);
        res.status(500).json({ error: err.message });
    }
});

// =========================================
// GET /api/users?sensor=AS608_1|AS608_2|AS608|R503
// =========================================
app.get('/api/users', async (req, res) => {
    let { sensor } = req.query;
    const normSensor = normalizeSensor(sensor);

    try {
        let fpCol = 'NULL AS fingerprint';
        if (normSensor === 'AS608') fpCol = 'fingerprint_as608 AS fingerprint';
        if (normSensor === 'R503')  fpCol = 'fingerprint_r503  AS fingerprint';

        let query = `SELECT id, slot, name, code, sensors, ${fpCol} FROM users`;
        let params = [];

        if (normSensor) {
            query += ` WHERE sensors LIKE ?`;
            params.push(`%${normSensor}%`);
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
// GET /api/template/:code
// =========================================
app.get('/api/template/:code', async (req, res) => {
    const code   = req.params.code;
    const sensor = req.query.sensor || 'AS608';
    const normSensor = normalizeSensor(sensor);
    const col = normSensor === 'R503' ? 'fingerprint_r503' : 'fingerprint_as608';

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
// =========================================
app.delete('/api/user/:code/sensor', async (req, res) => {
    const code   = req.params.code;
    let sensor = req.body.sensor;
    const normSensor = normalizeSensor(sensor);

    if (!normSensor) return res.status(400).json({ error: 'Thiếu sensor' });

    const col = normSensor === 'R503' ? 'fingerprint_r503' : 'fingerprint_as608';

    try {
        const [rows] = await db.execute(`SELECT sensors FROM users WHERE code = ?`, [code]);
        if (rows.length === 0) return res.status(404).json({ error: 'Không tìm thấy user' });

        let sensors = (rows[0].sensors || '').split(',').filter(s => normalizeSensor(s) !== normSensor).join(',');

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
// =========================================
// POST /api/log
// =========================================
app.post('/api/log', async (req, res) => {
    const { slot, uid, name, code, granted, sensor } = req.body;

    // Chuyển về chữ thường và đồng bộ dấu '-' thành '_' (ví dụ: as608-1 -> as608_1)
    let method = (sensor || 'fingerprint').toLowerCase().replace('-', '_');
    
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
                method, // Bây giờ sẽ lưu đúng 'as608_1' hoặc 'as608_2'
                granted ? 1 : 0
            ]
        );

        console.log(`[log] ${method} | code=${code} | ${name} | granted=${granted}`);
        res.status(200).json({ status: 'ok' });
    } catch (err) {
        console.error('[log] DB error:', err.message);
        res.status(500).json({ error: err.message });
    }
});

// =========================================
// GET /api/log?sensor=AS608|R503|rfid|remote
// =========================================
// =========================================
// GET /api/log?sensor=as608_1|as608_2|r503|rfid|remote
// =========================================
app.get('/api/log', async (req, res) => {
    const { sensor } = req.query; 
    
    try {
        let query = `SELECT id, user_id, uid, name, code, method, granted, 
                     DATE_FORMAT(created_at, '%H:%M:%S - %d/%m/%Y') AS formatted_time 
                     FROM access_logs`;
        let params = [];

        if (sensor && sensor !== 'all') {
            query += ` WHERE method = ?`;
            // Chuẩn hóa tham số tìm kiếm về chữ thường và đổi '-' thành '_' giống lúc lưu DB
            params.push(sensor.toLowerCase().replace('-', '_'));
        }

        query += ` ORDER BY id DESC LIMIT 20`; 

        const [rows] = await db.execute(query, params);
        
        const formattedLogs = rows.map(row => ({
            slot: row.user_id || 0,
            uid: row.uid || "",
            name: row.name,
            granted: row.granted === 1,
            time: row.formatted_time 
        }));

        res.status(200).json(formattedLogs);
    } catch (err) {
        console.error('[Get Log DB Error]:', err.message);
        res.status(500).json({ error: 'Lỗi truy vấn Database' });
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