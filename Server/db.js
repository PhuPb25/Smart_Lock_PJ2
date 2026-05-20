const mysql = require('mysql2/promise');

const pool = mysql.createPool({
    host:     'localhost',
    user:     'root',
    password: '',           // mặc định XAMPP
    database: 'smart_lock',
    waitForConnections: true,
    connectionLimit:    10,
});

pool.getConnection()
    .then(conn => {
        console.log('Kết nối Database thành công!');
        conn.release();
    })
    .catch(err => {
        console.error('Lỗi kết nối Database:', err.message);
    });

module.exports = pool;