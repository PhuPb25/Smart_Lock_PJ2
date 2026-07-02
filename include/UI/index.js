// =========================================
// STATE
// =========================================
let currentLogFilter  = 'as608-1';
let currentSyncSensor = 0;

function getApiKey() {
  return sessionStorage.getItem('api_key') || '';
}

function saveApiKey() {
  const key = document.getElementById('api_key_input').value.trim();
  if (!key) { toast('Nhập key trước', '#ef4444'); return; }
  sessionStorage.setItem('api_key', key);
  document.getElementById('key_status').innerText = 'Đã lưu API Key vào session trình duyệt';
  toast('API Key đã lưu', '#22c55e');
}

function authHeaders(extra) {
  return Object.assign({ 'X-API-Key': getApiKey() }, extra || {});
}

// =========================================
// UI HELPERS
// =========================================
function showTab(id, btn) {
  document.querySelectorAll('.tab').forEach(t => t.classList.remove('active'));
  document.getElementById(id).classList.add('active');
  document.querySelectorAll('.nav button').forEach(b => b.classList.remove('active'));
  if (btn) btn.classList.add('active');
  if (id === 'home')    loadStats();
  if (id === 'as608_1') loadAS1Users();
  if (id === 'as608_2') loadAS2Users();
  if (id === 'rfid')    loadRFIDList();
  if (id === 'log')     loadLog();
  if (id === 'db')      { loadDbUsers(); loadDbRfid(); }
}

function toast(msg, color) {
  const t = document.getElementById('toast');
  t.innerText = msg;
  t.style.background = color || '#22c55e';
  t.classList.add('show');
  setTimeout(() => t.classList.remove('show'), 2500);
}

// =========================================
// HOME
// =========================================
function loadStats() {
  fetch('/sync-status').then(r => r.json()).then(d => {
    document.getElementById('stat-as1').innerText = d.as608_1 ?? '—';
    document.getElementById('stat-as2').innerText = d.as608_2 ?? '—';
  }).catch(() => {});

  fetch('/rfid-list').then(r => r.json()).then(d => {
    document.getElementById('stat-rfid').innerText = d.length;
  }).catch(() => {});
}

function unlock() {
  const token = document.getElementById('unlock_token').value.trim();
  if (!token) { toast('Nhập token trước', '#ef4444'); return; }
  const icon = document.getElementById('lockIcon');
  icon.innerText = '⏳';
  fetch('/unlock', {
    method: 'POST',
    headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
    body: 'token=' + encodeURIComponent(token)
  }).then(r => r.json()).then(d => {
    if (d.status === 'unlocked') {
      icon.innerText = '🔓'; icon.style.borderColor = '#22c55e';
      toast('Đã mở khóa!', '#22c55e');
      setTimeout(() => { icon.innerText = '🔒'; icon.style.borderColor = '#3b82f6'; }, 3000);
    } else {
      icon.innerText = '🔒'; toast('Token sai!', '#ef4444');
    }
  }).catch(() => { icon.innerText = '🔒'; toast('Lỗi kết nối', '#ef4444'); });
}

function setSyncSensor(idx) {
  currentSyncSensor = idx;
  document.getElementById('syncTab0').classList.toggle('active', idx === 0);
  document.getElementById('syncTab1').classList.toggle('active', idx === 1);
}

function syncFromDB() {
  const btn  = document.getElementById('syncBtn');
  const st   = document.getElementById('syncStatus');
  const prog = document.getElementById('syncProgress');
  const ep   = currentSyncSensor === 0 ? '/as608-1/sync-from-db' : '/as608-2/sync-from-db';

  btn.disabled = true;
  btn.innerHTML = '<span class="spin"></span> Đang sync...';
  prog.style.width = '10%';

  fetch(ep, { method: 'POST', headers: authHeaders() }).then(r => r.text()).then(msg => {
    st.innerText = msg;
    let p = 10, polls = 0;
    const iv = setInterval(() => {
      polls++; p = Math.min(90, p + 8); prog.style.width = p + '%';
      fetch('/sync-status').then(r => r.json()).then(d => {
        const c = currentSyncSensor === 0 ? d.as608_1 : d.as608_2;
        st.innerText = 'Đang sync... (' + c + ' user trong flash)';
        if (polls >= 12) {
          clearInterval(iv); prog.style.width = '100%';
          btn.disabled = false; btn.innerHTML = '↓ SYNC TẤT CẢ';
          st.innerText = 'Hoàn tất — ' + c + ' user';
          toast('Sync xong!', '#22c55e');
          setTimeout(() => { prog.style.width = '0%'; }, 1000);
        }
      });
    }, 2000);
  }).catch(() => {
    toast('Sync thất bại — kiểm tra API Key', '#ef4444');
    btn.disabled = false; btn.innerHTML = '↓ SYNC TẤT CẢ'; prog.style.width = '0%';
  });
}

// =========================================
// AS608 #1
// =========================================
function enrollAS1() {
  const code = document.getElementById('as1_code').value.trim();
  const name = document.getElementById('as1_name').value.trim();
  const st   = document.getElementById('as1_enroll_status');
  if (!code || !name) { toast('Thiếu mã số hoặc tên', '#ef4444'); return; }
  if (!getApiKey())   { toast('Chưa nhập API Key (tab Cài đặt)', '#ef4444'); return; }
  st.innerHTML = '<span class="spin"></span> Đang enroll — đặt ngón tay lên AS608 #1...';
  fetch('/as608-1/enroll', {
    method: 'POST',
    headers: authHeaders({ 'Content-Type': 'application/x-www-form-urlencoded' }),
    body: 'code=' + encodeURIComponent(code) + '&name=' + encodeURIComponent(name)
  }).then(r => r.text()).then(d => {
    st.innerText = d;
    toast(d, d.includes('thất bại') ? '#ef4444' : '#22c55e');
    if (!d.includes('thất bại')) {
      document.getElementById('as1_code').value = '';
      document.getElementById('as1_name').value = '';
      loadAS1Users();
    }
  }).catch(() => { st.innerText = 'Lỗi kết nối'; toast('Lỗi', '#ef4444'); });
}

function syncOneAS1() {
  const code = document.getElementById('as1_sync_code').value.trim();
  const st   = document.getElementById('as1_sync_status');
  if (!code)        { toast('Nhập mã số', '#ef4444'); return; }
  if (!getApiKey()) { toast('Chưa nhập API Key', '#ef4444'); return; }
  st.innerHTML = '<span class="spin"></span> Đang sync...';
  fetch('/as608-1/sync-from-db', {
    method: 'POST',
    headers: authHeaders({ 'Content-Type': 'application/json' }),
    body: JSON.stringify({ code })
  }).then(r => r.text()).then(d => { st.innerText = d; toast(d, '#3b82f6'); })
    .catch(() => { st.innerText = 'Lỗi'; toast('Lỗi', '#ef4444'); });
}

function syncAllAS1() {
  const st = document.getElementById('as1_sync_status');
  if (!getApiKey()) { toast('Chưa nhập API Key', '#ef4444'); return; }
  st.innerHTML = '<span class="spin"></span> Đang sync tất cả...';
  fetch('/as608-1/sync-from-db', { method: 'POST', headers: authHeaders() })
    .then(r => r.text()).then(d => { st.innerText = d; toast(d, '#3b82f6'); })
    .catch(() => { st.innerText = 'Lỗi'; toast('Lỗi', '#ef4444'); });
}

function loadAS1Users() {
  fetch('/as608-1/users').then(r => r.json()).then(data => {
    if (!data.length) {
      document.getElementById('as1_list').innerHTML = '<div class="empty">Chưa có user nào</div>';
      return;
    }
    document.getElementById('as1_list').innerHTML = data.map(u => `
      <div class="item">
        <div class="info"><b>${u.name}</b><span>Mã: ${u.code} · Slot: ${u.slot}</span></div>
        <div class="acts">
          <input class="edit-in" id="as1_e_${u.slot}" value="${u.name}">
          <button class="btn update" style="padding:5px 10px;font-size:12px" onclick="updateAS1(${u.slot})">Sửa</button>
          <button class="btn delete" style="padding:5px 10px;font-size:12px" onclick="deleteAS1(${u.slot},'${u.code}')">Xóa</button>
        </div>
      </div>`).join('');
  }).catch(() => {
    document.getElementById('as1_list').innerHTML = '<div class="empty" style="color:#ef4444">Lỗi tải dữ liệu</div>';
  });
}

function updateAS1(slot) {
  const name = document.getElementById('as1_e_' + slot).value;
  fetch('/as608-1/update', {
    method: 'POST',
    headers: authHeaders({ 'Content-Type': 'application/x-www-form-urlencoded' }),
    body: 'slot=' + slot + '&name=' + encodeURIComponent(name)
  }).then(r => r.text()).then(d => { toast(d, '#22c55e'); loadAS1Users(); });
}

function deleteAS1(slot, code) {
  if (!confirm('Xóa slot ' + slot + '?')) return;
  fetch('/as608-1/delete', {
    method: 'POST',
    headers: authHeaders({ 'Content-Type': 'application/x-www-form-urlencoded' }),
    body: 'slot=' + slot + '&code=' + encodeURIComponent(code)
  }).then(r => r.text()).then(d => { toast(d, '#22c55e'); loadAS1Users(); });
}

// =========================================
// AS608 #2
// =========================================
function enrollAS2() {
  const code = document.getElementById('as2_code').value.trim();
  const name = document.getElementById('as2_name').value.trim();
  const st   = document.getElementById('as2_enroll_status');
  if (!code || !name) { toast('Thiếu mã số hoặc tên', '#ef4444'); return; }
  if (!getApiKey())   { toast('Chưa nhập API Key (tab Cài đặt)', '#ef4444'); return; }
  st.innerHTML = '<span class="spin"></span> Đang enroll — đặt ngón tay lên AS608 #2...';
  fetch('/as608-2/enroll', {
    method: 'POST',
    headers: authHeaders({ 'Content-Type': 'application/x-www-form-urlencoded' }),
    body: 'code=' + encodeURIComponent(code) + '&name=' + encodeURIComponent(name)
  }).then(r => r.text()).then(d => {
    st.innerText = d;
    toast(d, d.includes('thất bại') ? '#ef4444' : '#22c55e');
    if (!d.includes('thất bại')) {
      document.getElementById('as2_code').value = '';
      document.getElementById('as2_name').value = '';
      loadAS2Users();
    }
  }).catch(() => { st.innerText = 'Lỗi kết nối'; toast('Lỗi', '#ef4444'); });
}

function syncOneAS2() {
  const code = document.getElementById('as2_sync_code').value.trim();
  const st   = document.getElementById('as2_sync_status');
  if (!code)        { toast('Nhập mã số', '#ef4444'); return; }
  if (!getApiKey()) { toast('Chưa nhập API Key', '#ef4444'); return; }
  st.innerHTML = '<span class="spin"></span> Đang sync...';
  fetch('/as608-2/sync-from-db', {
    method: 'POST',
    headers: authHeaders({ 'Content-Type': 'application/json' }),
    body: JSON.stringify({ code })
  }).then(r => r.text()).then(d => { st.innerText = d; toast(d, '#8b5cf6'); })
    .catch(() => { st.innerText = 'Lỗi'; toast('Lỗi', '#ef4444'); });
}

function syncAllAS2() {
  const st = document.getElementById('as2_sync_status');
  if (!getApiKey()) { toast('Chưa nhập API Key', '#ef4444'); return; }
  st.innerHTML = '<span class="spin"></span> Đang sync tất cả...';
  fetch('/as608-2/sync-from-db', { method: 'POST', headers: authHeaders() })
    .then(r => r.text()).then(d => { st.innerText = d; toast(d, '#8b5cf6'); })
    .catch(() => { st.innerText = 'Lỗi'; toast('Lỗi', '#ef4444'); });
}

function loadAS2Users() {
  fetch('/as608-2/users').then(r => r.json()).then(data => {
    if (!data.length) {
      document.getElementById('as2_list').innerHTML = '<div class="empty">Chưa có user nào</div>';
      return;
    }
    document.getElementById('as2_list').innerHTML = data.map(u => `
      <div class="item">
        <div class="info"><b>${u.name}</b><span>Mã: ${u.code} · Slot: ${u.slot}</span></div>
        <div class="acts">
          <input class="edit-in" id="as2_e_${u.slot}" value="${u.name}">
          <button class="btn update" style="padding:5px 10px;font-size:12px" onclick="updateAS2(${u.slot})">Sửa</button>
          <button class="btn delete" style="padding:5px 10px;font-size:12px" onclick="deleteAS2(${u.slot},'${u.code}')">Xóa</button>
        </div>
      </div>`).join('');
  }).catch(() => {
    document.getElementById('as2_list').innerHTML = '<div class="empty" style="color:#ef4444">Lỗi tải dữ liệu</div>';
  });
}

function updateAS2(slot) {
  const name = document.getElementById('as2_e_' + slot).value;
  fetch('/as608-2/update', {
    method: 'POST',
    headers: authHeaders({ 'Content-Type': 'application/x-www-form-urlencoded' }),
    body: 'slot=' + slot + '&name=' + encodeURIComponent(name)
  }).then(r => r.text()).then(d => { toast(d, '#22c55e'); loadAS2Users(); });
}

function deleteAS2(slot, code) {
  if (!confirm('Xóa slot ' + slot + '?')) return;
  fetch('/as608-2/delete', {
    method: 'POST',
    headers: authHeaders({ 'Content-Type': 'application/x-www-form-urlencoded' }),
    body: 'slot=' + slot + '&code=' + encodeURIComponent(code)
  }).then(r => r.text()).then(d => { toast(d, '#22c55e'); loadAS2Users(); });
}

// =========================================
// RFID
// =========================================
function scanRFID() {
  const st = document.getElementById('rfid_add_status');
  st.innerHTML = '<span class="spin"></span> Đang quét — đưa thẻ lại gần...';
  fetch('/scan-rfid').then(r => r.text()).then(uid => {
    if (uid.includes('Timeout')) {
      st.innerText = 'Timeout — không tìm thấy thẻ'; toast('Không thấy thẻ', '#ef4444');
    } else {
      document.getElementById('rfid_uid').value = uid;
      st.innerText = 'Đã quét: ' + uid; toast('Đã quét: ' + uid, '#22c55e');
    }
  }).catch(() => { st.innerText = 'Lỗi'; toast('Lỗi', '#ef4444'); });
}

function addRFID() {
  const uid  = document.getElementById('rfid_uid').value.trim();
  const name = document.getElementById('rfid_name').value.trim();
  const st   = document.getElementById('rfid_add_status');
  if (!uid || !name) { toast('Thiếu UID hoặc tên', '#ef4444'); return; }
  if (!getApiKey())  { toast('Chưa nhập API Key', '#ef4444'); return; }
  fetch('/add-rfid', {
    method: 'POST',
    headers: authHeaders({ 'Content-Type': 'application/x-www-form-urlencoded' }),
    body: 'uid=' + encodeURIComponent(uid) + '&name=' + encodeURIComponent(name)
  }).then(r => r.text()).then(d => {
    st.innerText = d; toast(d, '#22c55e');
    document.getElementById('rfid_uid').value  = '';
    document.getElementById('rfid_name').value = '';
    loadRFIDList();
  });
}

function loadRFIDList() {
  fetch('/rfid-list').then(r => r.json()).then(data => {
    if (!data.length) {
      document.getElementById('rfid_list').innerHTML = '<div class="empty">Chưa có thẻ nào</div>';
      return;
    }
    document.getElementById('rfid_list').innerHTML = data.map(r => `
      <div class="item">
        <div class="info"><b>${r.name}</b><span>UID: ${r.uid}</span></div>
        <div class="acts">
          <input class="edit-in" id="rfid_e_${r.id}" value="${r.name}">
          <button class="btn update" style="padding:5px 10px;font-size:12px" onclick="updateRFID(${r.id})">Sửa</button>
          <button class="btn delete" style="padding:5px 10px;font-size:12px" onclick="deleteRFID(${r.id})">Xóa</button>
        </div>
      </div>`).join('');
  }).catch(() => {
    document.getElementById('rfid_list').innerHTML = '<div class="empty" style="color:#ef4444">Lỗi tải dữ liệu</div>';
  });
}

function updateRFID(id) {
  const name = document.getElementById('rfid_e_' + id).value;
  fetch('/rfid-update', {
    method: 'POST',
    headers: authHeaders({ 'Content-Type': 'application/x-www-form-urlencoded' }),
    body: 'id=' + id + '&name=' + encodeURIComponent(name)
  }).then(r => r.text()).then(d => { toast(d, '#22c55e'); loadRFIDList(); });
}

function deleteRFID(id) {
  if (!confirm('Xóa thẻ này?')) return;
  fetch('/rfid-delete', {
    method: 'POST',
    headers: authHeaders({ 'Content-Type': 'application/x-www-form-urlencoded' }),
    body: 'id=' + id
  }).then(r => r.text()).then(d => { toast(d, '#22c55e'); loadRFIDList(); });
}

// =========================================
// LOG
// =========================================
const logMethodBadge = {
  'as608-1': '<span class="badge b-blue">AS608 #1</span>',
  'as608-2': '<span class="badge b-purple">AS608 #2</span>',
  'rfid':    '<span class="badge b-cyan">RFID</span>',
  'remote':  '<span class="badge b-orange">Remote</span>',
};

function setLogFilter(f, btn) {
  currentLogFilter = f;
  document.querySelectorAll('.ftab').forEach(b => b.classList.remove('active'));
  if (btn) btn.classList.add('active');
  loadLog();
}

function refreshCurrentLogs() {
  loadLog();
}

function loadLog() {
  const datePicker   = document.getElementById('log_date_filter');
  const selectedDate = datePicker ? datePicker.value : '';
  let url = '/log-proxy?sensor=' + currentLogFilter;
  if (selectedDate) url += '&date=' + selectedDate;

  fetch(url)
    .then(r => { if (!r.ok) throw new Error(r.status); return r.json(); })
    .then(data => {
      const el = document.getElementById('log_list');
      if (!data.length) {
        el.innerHTML = '<div class="empty">Không có nhật ký truy cập trong ngày này</div>';
        return;
      }
      el.innerHTML = data.map(l => {
        const methodKey = l.method || currentLogFilter;
        const badge = logMethodBadge[methodKey] || '';
        return `
          <div class="log-item ${l.granted ? 'ok' : 'err'}">
            <div style="font-size:18px">${l.granted ? '✅' : '❌'}</div>
            <div class="log-info">
              <b>${l.name || 'Unknown'} ${badge}</b>
              <span>${l.slot ? 'Slot: ' + l.slot : ''} ${l.uid ? ' · UID: ' + l.uid : ''}</span>
            </div>
            <div class="log-time">${l.time || ''}</div>
          </div>`;
      }).join('');
    }).catch(() => {
      document.getElementById('log_list').innerHTML =
        '<div class="empty" style="color:#ef4444">Lỗi tải log — kiểm tra kết nối server</div>';
    });
}

// =========================================
// DATABASE
// =========================================
function loadDbUsers() {
  fetch('/db-users').then(r => r.json()).then(data => {
    if (!data.length) {
      document.getElementById('db_user_list').innerHTML = '<div class="empty">Chưa có user nào trên server</div>';
      return;
    }
    document.getElementById('db_user_list').innerHTML = data.map(u => {
      const hasAS  = u.fingerprint_as608 && u.fingerprint_as608.length > 0;
      const sensors = u.sensors || '—';
      return `
        <div class="item">
          <div class="info">
            <b>${u.name}</b>
            <span>Mã: ${u.code || '—'} · Slot: ${u.slot || '—'} · <span class="badge b-blue">${sensors}</span></span>
            <div class="fp-row">
              <span class="fp-chip ${hasAS ? 'fp-ok' : 'fp-no'}">AS608 ${hasAS ? '✓' : '✗'}</span>
            </div>
          </div>
          <div class="acts">
            <input class="edit-in" id="db_e_${u.code}" value="${u.name}">
            <button class="btn update" style="padding:5px 10px;font-size:12px" onclick="dbUpdateUser('${u.code}')">Sửa</button>
          </div>
        </div>`;
    }).join('');
  }).catch(() => {
    document.getElementById('db_user_list').innerHTML =
      '<div class="empty" style="color:#ef4444">Không kết nối được server</div>';
  });
}

function loadDbRfid() {
  fetch('/db-rfid').then(r => r.json()).then(data => {
    if (!data.length) {
      document.getElementById('db_rfid_list').innerHTML = '<div class="empty">Chưa có thẻ RFID nào trên server</div>';
      return;
    }
    document.getElementById('db_rfid_list').innerHTML = data.map(r => `
      <div class="item">
        <div class="info"><b>${r.name}</b><span>UID: ${r.uid}</span></div>
        <div class="acts">
          <input class="edit-in" id="db_rfid_${r.uid}" value="${r.name}">
          <button class="btn update" style="padding:5px 10px;font-size:12px" onclick="dbUpdateRfid('${r.uid}')">Sửa</button>
        </div>
      </div>`).join('');
  }).catch(() => {
    document.getElementById('db_rfid_list').innerHTML =
      '<div class="empty" style="color:#ef4444">Không kết nối được server</div>';
  });
}

function dbUpdateUser(code) {
  const name = document.getElementById('db_e_' + code).value;
  if (!getApiKey()) { toast('Chưa nhập API Key', '#ef4444'); return; }
  fetch('/db-users', {
    method: 'POST',
    headers: authHeaders({ 'Content-Type': 'application/json' }),
    body: JSON.stringify({ code, name })
  }).then(r => r.json()).then(() => { toast('Đã cập nhật server', '#22c55e'); loadDbUsers(); })
    .catch(() => toast('Lỗi cập nhật', '#ef4444'));
}

function dbUpdateRfid(uid) {
  const name = document.getElementById('db_rfid_' + uid).value;
  if (!getApiKey()) { toast('Chưa nhập API Key', '#ef4444'); return; }
  fetch('/db-rfid', {
    method: 'POST',
    headers: authHeaders({ 'Content-Type': 'application/json' }),
    body: JSON.stringify({ uid, name })
  }).then(r => r.json()).then(() => { toast('Đã cập nhật RFID', '#22c55e'); loadDbRfid(); })
    .catch(() => toast('Lỗi cập nhật', '#ef4444'));
}

// =========================================
// SETTINGS
// =========================================
function updateServerIP() {
  const ip = document.getElementById('server_ip_input').value.trim();
  if (!ip)          { toast('Nhập IP trước', '#ef4444'); return; }
  if (!getApiKey()) { toast('Chưa nhập API Key', '#ef4444'); return; }
  fetch('/set-server-ip', {
    method: 'POST',
    headers: authHeaders({ 'Content-Type': 'text/plain' }),
    body: ip
  }).then(r => r.text()).then(d => {
    document.getElementById('ip_status').innerText = d;
    toast(d, '#22c55e');
  }).catch(() => toast('Lỗi', '#ef4444'));
}

// =========================================
// KHỞI TẠO
// =========================================
loadStats();