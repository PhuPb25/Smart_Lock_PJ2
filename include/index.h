#include <Arduino.h>

#ifndef INDEX_HTML_H
#define INDEX_HTML_H

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Smart Lock Pro</title>
<link href="https://fonts.googleapis.com/css2?family=Poppins:wght@400;500;600&display=swap" rel="stylesheet">
<style>
* { font-family: 'Poppins', sans-serif; box-sizing: border-box; margin: 0; padding: 0; }
body { background: linear-gradient(135deg, #0f172a, #1e293b); color: white; min-height: 100vh; }

.nav { display: flex; background: rgba(255,255,255,0.05); border-bottom: 1px solid rgba(255,255,255,0.08); overflow-x: auto; }
.nav button { flex: 1; min-width: 70px; padding: 14px 8px; border: none; background: transparent; color: rgba(255,255,255,0.6); cursor: pointer; font-size: 12px; white-space: nowrap; transition: all .2s; }
.nav button.active { background: #3b82f6; color: white; }
.nav button:hover:not(.active) { background: rgba(255,255,255,0.07); color: white; }

.tab { display: none; padding: 15px; }
.tab.active { display: block; }

.card { background: rgba(255,255,255,0.05); padding: 20px; border-radius: 12px; margin: 10px 0; border: 1px solid rgba(255,255,255,0.06); }
.card h2 { font-size: 15px; font-weight: 600; margin-bottom: 14px; display: flex; align-items: center; gap: 8px; }

.grid2 { display: grid; grid-template-columns: 1fr 1fr; gap: 10px; }
@media(max-width:600px) { .grid2 { grid-template-columns: 1fr; } }

.stat-grid { display: grid; grid-template-columns: repeat(3,1fr); gap: 10px; margin-bottom: 10px; }
@media(max-width:500px) { .stat-grid { grid-template-columns: 1fr; } }
.stat-card { background: rgba(255,255,255,0.05); border-radius: 12px; padding: 16px; text-align: center; border: 1px solid rgba(255,255,255,0.06); }
.stat-num { font-size: 32px; font-weight: 600; line-height: 1; }
.stat-lbl { font-size: 12px; opacity: .5; margin-top: 4px; }

input[type=text], input[type=password] { width: 100%; padding: 10px; margin: 5px 0; border-radius: 8px; border: 1px solid rgba(255,255,255,0.1); background: rgba(255,255,255,0.07); color: white; font-size: 13px; outline: none; transition: border-color .2s; }
input[type=text]:focus, input[type=password]:focus { border-color: #3b82f6; }
input::placeholder { color: rgba(255,255,255,0.3); }

.btn { padding: 9px 14px; border: none; border-radius: 8px; margin: 4px 2px; color: white; cursor: pointer; font-size: 13px; font-family: 'Poppins', sans-serif; font-weight: 500; transition: opacity .15s, transform .1s; display: inline-flex; align-items: center; gap: 5px; }
.btn:active { transform: scale(.97); }
.btn:disabled { opacity: .4; cursor: not-allowed; }
.open   { background: #22c55e; }
.enroll { background: #3b82f6; }
.purple { background: #8b5cf6; }
.scan   { background: #06b6d4; }
.update { background: #facc15; color: #000; }
.delete { background: #ef4444; }
.ghost  { background: rgba(255,255,255,0.1); }
.yellow { background: #f59e0b; }
.orange { background: #f97316; }

.stab-row { display: flex; gap: 6px; margin-bottom: 14px; }
.stab { flex: 1; padding: 8px; border: 1px solid rgba(255,255,255,0.1); background: transparent; color: rgba(255,255,255,0.5); border-radius: 8px; cursor: pointer; font-size: 12px; font-family: 'Poppins', sans-serif; font-weight: 500; transition: all .2s; }
.stab.active { background: #3b82f6; border-color: #3b82f6; color: white; }

.item { background: rgba(255,255,255,0.06); padding: 12px; border-radius: 10px; margin: 6px 0; display: flex; align-items: center; gap: 10px; }
.item .info { flex: 1; min-width: 0; font-size: 13px; }
.item .info b { display: block; white-space: nowrap; overflow: hidden; text-overflow: ellipsis; }
.item .info span { opacity: .5; font-size: 11px; }
.item .acts { display: flex; gap: 4px; flex-shrink: 0; flex-wrap: wrap; justify-content: flex-end; }

.edit-in { background: rgba(255,255,255,0.1); border: 1px solid rgba(255,255,255,0.15); border-radius: 6px; padding: 5px 8px; color: white; font-size: 12px; width: 110px; outline: none; }
.edit-in:focus { border-color: #3b82f6; }

.log-item { background: rgba(255,255,255,0.05); border-left: 3px solid rgba(255,255,255,0.2); padding: 10px 12px; border-radius: 0 8px 8px 0; margin: 5px 0; font-size: 13px; display: flex; align-items: center; gap: 10px; }
.log-item.ok  { border-left-color: #22c55e; }
.log-item.err { border-left-color: #ef4444; }
.log-info { flex: 1; }
.log-info b { display: block; }
.log-info span { font-size: 11px; opacity: .5; }
.log-time { font-size: 11px; opacity: .5; text-align: right; flex-shrink: 0; }

.badge { font-size: 11px; padding: 2px 8px; border-radius: 20px; display: inline-block; }
.b-green  { background: rgba(34,197,94,.2);  color: #22c55e; }
.b-red    { background: rgba(239,68,68,.2);   color: #ef4444; }
.b-blue   { background: rgba(59,130,246,.2);  color: #93c5fd; }
.b-purple { background: rgba(139,92,246,.2);  color: #c4b5fd; }
.b-cyan   { background: rgba(6,182,212,.2);   color: #67e8f9; }
.b-orange { background: rgba(249,115,22,.2);  color: #fdba74; }

.status { font-size: 12px; opacity: .6; margin-top: 8px; min-height: 18px; }
.prog-wrap { height: 4px; background: rgba(255,255,255,0.1); border-radius: 4px; overflow: hidden; margin-top: 8px; }
.prog-bar  { height: 100%; background: #3b82f6; border-radius: 4px; width: 0%; transition: width .3s; }

.lock-wrap { text-align: center; padding: 20px 0 10px; }
.lock-circle { width: 72px; height: 72px; border-radius: 50%; background: rgba(59,130,246,.1); border: 2px solid #3b82f6; display: inline-flex; align-items: center; justify-content: center; font-size: 28px; margin-bottom: 12px; animation: pulse 3s infinite; }
@keyframes pulse { 0%,100% { box-shadow: 0 0 0 0 rgba(59,130,246,.3); } 50% { box-shadow: 0 0 0 14px rgba(59,130,246,0); } }

.filter-row { display: flex; gap: 6px; margin-bottom: 12px; flex-wrap: wrap; }
.ftab { border: 1px solid rgba(255,255,255,0.15); background: transparent; color: rgba(255,255,255,0.5); padding: 5px 14px; border-radius: 20px; font-size: 12px; font-family: 'Poppins', sans-serif; cursor: pointer; transition: all .15s; }
.ftab.active { background: #3b82f6; border-color: #3b82f6; color: white; }

.spin { display: inline-block; width: 12px; height: 12px; border: 2px solid rgba(255,255,255,.25); border-top-color: white; border-radius: 50%; animation: sp .6s linear infinite; vertical-align: middle; }
@keyframes sp { to { transform: rotate(360deg); } }

.empty { text-align: center; padding: 30px; opacity: .4; font-size: 13px; }
.note  { font-size: 11px; opacity: .45; margin-top: 6px; line-height: 1.6; }

#toast { position: fixed; bottom: 20px; left: 50%; transform: translateX(-50%) translateY(60px); padding: 10px 20px; border-radius: 10px; font-size: 13px; z-index: 999; transition: transform .3s, opacity .3s; opacity: 0; pointer-events: none; }
#toast.show { transform: translateX(-50%) translateY(0); opacity: 1; }
</style>
</head>
<body>

<div class="nav">
  <button onclick="showTab('home')" class="active">🏠 Home</button>
  <button onclick="showTab('as608')">👁 AS608</button>
  <button onclick="showTab('r558s')">👁 R558S</button>
  <button onclick="showTab('rfid')">📡 RFID</button>
  <button onclick="showTab('log')">📜 Log</button>
  <button onclick="showTab('db')">☁️ Server</button>
  <button onclick="showTab('settings')">⚙️ Cài đặt</button>
</div>

<!-- HOME -->
<div id="home" class="tab active">
  <div class="stat-grid">
    <div class="stat-card">
      <div class="stat-num" id="stat-as" style="color:#3b82f6">—</div>
      <div class="stat-lbl">AS608 users</div>
    </div>
    <div class="stat-card">
      <div class="stat-num" id="stat-rs" style="color:#8b5cf6">—</div>
      <div class="stat-lbl">R558S users</div>
    </div>
    <div class="stat-card">
      <div class="stat-num" id="stat-rfid" style="color:#22c55e">—</div>
      <div class="stat-lbl">Thẻ RFID</div>
    </div>
  </div>

  <div class="grid2">
    <div class="card">
      <h2>🔓 Mở khóa từ xa</h2>
      <div class="lock-wrap"><div class="lock-circle" id="lockIcon">🔒</div></div>
      <input type="password" id="unlock_token" placeholder="Nhập token...">
      <button class="btn open" style="width:100%;margin-top:8px" onclick="unlock()">🔓 MỞ KHÓA</button>
    </div>

    <div class="card">
      <h2>☁️ Đồng bộ từ server</h2>
      <p class="note">Kéo toàn bộ vân tay từ database về thiết bị. Chọn cảm biến cần sync.</p>
      <div class="stab-row" style="margin-top:10px">
        <button class="stab active" id="syncTab0" onclick="setSyncSensor(0)">AS608</button>
        <button class="stab" id="syncTab1" onclick="setSyncSensor(1)">R558S</button>
      </div>
      <button class="btn enroll" id="syncBtn" style="width:100%" onclick="syncFromDB()">↓ SYNC TẤT CẢ</button>
      <div class="prog-wrap"><div class="prog-bar" id="syncProgress"></div></div>
      <div class="status" id="syncStatus"></div>
    </div>
  </div>
</div>

<!-- AS608 -->
<div id="as608" class="tab">
  <div class="grid2">
    <div class="card">
      <h2>➕ Enroll vân tay AS608</h2>
      <input type="text" id="as_code" placeholder="Mã số (VD: SV001)">
      <input type="text" id="as_name" placeholder="Tên người dùng">
      <button class="btn enroll" onclick="enrollAS()">+ ENROLL</button>
      <div class="status" id="as_enroll_status"></div>
    </div>
    <div class="card">
      <h2>☁️ Sync từ server</h2>
      <input type="text" id="as_sync_code" placeholder="Mã số cần sync (để trống = tất cả)">
      <button class="btn yellow" onclick="syncOneAS()">↓ Sync 1 user</button>
      <button class="btn ghost" onclick="syncAllAS()">↓ Sync tất cả</button>
      <div class="status" id="as_sync_status"></div>
    </div>
  </div>
  <div class="card">
    <h2>
      👤 Danh sách user AS608
      <button class="btn ghost" style="padding:5px 10px;font-size:11px;margin-left:auto" onclick="loadAS608Users()">↻ Làm mới</button>
    </h2>
    <div id="as608_list"><div class="empty">Đang tải...</div></div>
  </div>
</div>

<!-- R558S -->
<div id="r558s" class="tab">
  <div class="grid2">
    <div class="card">
      <h2>➕ Enroll vân tay R558S</h2>
      <input type="text" id="rs_code" placeholder="Mã số (VD: SV001)">
      <input type="text" id="rs_name" placeholder="Tên người dùng">
      <button class="btn purple" onclick="enrollRS()">+ ENROLL</button>
      <div class="status" id="rs_enroll_status"></div>
    </div>
    <div class="card">
      <h2>☁️ Sync từ server</h2>
      <input type="text" id="rs_sync_code" placeholder="Mã số cần sync (để trống = tất cả)">
      <button class="btn yellow" onclick="syncOneRS()">↓ Sync 1 user</button>
      <button class="btn ghost" onclick="syncAllRS()">↓ Sync tất cả</button>
      <div class="status" id="rs_sync_status"></div>
    </div>
  </div>
  <div class="card">
    <h2>
      👤 Danh sách user R558S
      <button class="btn ghost" style="padding:5px 10px;font-size:11px;margin-left:auto" onclick="loadR558SUsers()">↻ Làm mới</button>
    </h2>
    <div id="r558s_list"><div class="empty">Đang tải...</div></div>
  </div>
</div>

<!-- RFID -->
<div id="rfid" class="tab">
  <div class="card">
    <h2>📡 Thêm thẻ RFID</h2>
    <input type="text" id="rfid_uid" placeholder="UID (VD: D1F44453)">
    <input type="text" id="rfid_name" placeholder="Tên người dùng">
    <button class="btn scan" onclick="scanRFID()">📡 Quét thẻ</button>
    <button class="btn open" onclick="addRFID()">+ ADD</button>
    <div class="status" id="rfid_add_status"></div>
  </div>
  <div class="card">
    <h2>
      📋 Danh sách thẻ
      <button class="btn ghost" style="padding:5px 10px;font-size:11px;margin-left:auto" onclick="loadRFIDList()">↻ Làm mới</button>
    </h2>
    <div id="rfid_list"><div class="empty">Đang tải...</div></div>
  </div>
</div>

<!-- LOG -->
<div id="log" class="tab">
  <div class="card">
    <h2>📜 Lịch sử truy cập</h2>
    <div class="filter-row">
      <button class="ftab active" onclick="setLogFilter('as608')">AS608</button>
      <button class="ftab" onclick="setLogFilter('r558s')">R558S</button>
      <button class="ftab" onclick="setLogFilter('rfid')">RFID</button>
      <button class="ftab" onclick="setLogFilter('remote')">Remote</button>
    </div>
    <button class="btn ghost" style="margin-bottom:10px;font-size:12px" onclick="loadLog()">↻ Làm mới</button>
    <div id="log_list"><div class="empty">Đang tải...</div></div>
  </div>
</div>

<!-- SERVER/DATABASE -->
<div id="db" class="tab">
  <div class="card">
    <h2>
      ☁️ Users trên server
      <button class="btn ghost" style="padding:5px 10px;font-size:11px;margin-left:auto" onclick="loadDbUsers()">↻</button>
    </h2>
    <div id="db_user_list"><div class="empty">Đang tải...</div></div>
  </div>
  <div class="card">
    <h2>
      📡 RFID trên server
      <button class="btn ghost" style="padding:5px 10px;font-size:11px;margin-left:auto" onclick="loadDbRfid()">↻</button>
    </h2>
    <div id="db_rfid_list"><div class="empty">Đang tải...</div></div>
  </div>
</div>

<!-- SETTINGS -->
<div id="settings" class="tab">
  <div class="card">
    <h2>🔑 API Key</h2>
    <p class="note">API Key bảo vệ các thao tác ghi (enroll, xóa, sync). Lưu key này và điền vào mỗi lần dùng giao diện trên trình duyệt mới.</p>
    <input type="password" id="api_key_input" placeholder="Nhập API Key của thiết bị...">
    <button class="btn enroll" onclick="saveApiKey()">💾 Lưu key</button>
    <div class="status" id="key_status"></div>
  </div>
  <div class="card">
    <h2>🌐 Địa chỉ server Node.js</h2>
    <p class="note">Địa chỉ IP máy chủ chạy server.js (thay đổi khi IP thay đổi).</p>
    <input type="text" id="server_ip_input" placeholder="VD: 192.168.1.2">
    <button class="btn yellow" onclick="updateServerIP()">🔄 Cập nhật IP</button>
    <div class="status" id="ip_status"></div>
  </div>
</div>

<div id="toast"></div>

<script>
// =========================================
// STATE
// =========================================
let currentLogFilter  = 'as608';
let currentSyncSensor = 0;

// API Key — lưu trong sessionStorage của trình duyệt
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

// Headers mặc định cho request cần auth
function authHeaders(extra) {
  return Object.assign({'X-API-Key': getApiKey()}, extra || {});
}

// =========================================
// UI HELPERS
// =========================================
function showTab(id) {
  document.querySelectorAll('.tab').forEach(t => t.classList.remove('active'));
  document.getElementById(id).classList.add('active');
  document.querySelectorAll('.nav button').forEach(b => b.classList.remove('active'));
  event.target.classList.add('active');
  if (id==='home')     loadStats();
  if (id==='as608')    loadAS608Users();
  if (id==='r558s')    loadR558SUsers();
  if (id==='rfid')     loadRFIDList();
  if (id==='log')      loadLog();
  if (id==='db')     { loadDbUsers(); loadDbRfid(); }
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
  fetch('/sync-status').then(r=>r.json()).then(d=>{
    document.getElementById('stat-as').innerText   = d.as608 ?? '—';
    document.getElementById('stat-rs').innerText   = d.r558s ?? '—';
  }).catch(()=>{});
  fetch('/rfid-list').then(r=>r.json()).then(d=>{
    document.getElementById('stat-rfid').innerText = d.length;
  }).catch(()=>{});
}

function unlock() {
  const token = document.getElementById('unlock_token').value.trim();
  if (!token) { toast('Nhập token trước', '#ef4444'); return; }
  const icon = document.getElementById('lockIcon');
  icon.innerText = '⏳';
  fetch('/unlock', {
    method:'POST',
    headers:{'Content-Type':'application/x-www-form-urlencoded'},
    body:'token='+encodeURIComponent(token)
  }).then(r=>r.json()).then(d=>{
    if (d.status==='unlocked') {
      icon.innerText='🔓'; icon.style.borderColor='#22c55e';
      toast('Đã mở khóa!','#22c55e');
      setTimeout(()=>{icon.innerText='🔒';icon.style.borderColor='#3b82f6';},3000);
    } else {
      icon.innerText='🔒'; toast('Token sai!','#ef4444');
    }
  }).catch(()=>{ icon.innerText='🔒'; toast('Lỗi kết nối','#ef4444'); });
}

function setSyncSensor(idx) {
  currentSyncSensor = idx;
  document.getElementById('syncTab0').classList.toggle('active', idx===0);
  document.getElementById('syncTab1').classList.toggle('active', idx===1);
}

function syncFromDB() {
  const btn  = document.getElementById('syncBtn');
  const st   = document.getElementById('syncStatus');
  const prog = document.getElementById('syncProgress');
  const ep   = currentSyncSensor===0 ? '/as608/sync-from-db' : '/r558s/sync-from-db';
  btn.disabled=true; btn.innerHTML='<span class="spin"></span> Đang sync...';
  prog.style.width='10%';
  fetch(ep, {method:'POST', headers: authHeaders()}).then(r=>r.text()).then(msg=>{
    st.innerText=msg;
    let p=10, polls=0;
    const iv=setInterval(()=>{
      polls++; p=Math.min(90,p+8); prog.style.width=p+'%';
      fetch('/sync-status').then(r=>r.json()).then(d=>{
        const c=currentSyncSensor===0?d.as608:d.r558s;
        st.innerText='Đang sync... ('+c+' user trong flash)';
        if(polls>=12){
          clearInterval(iv); prog.style.width='100%';
          btn.disabled=false; btn.innerHTML='↓ SYNC TẤT CẢ';
          st.innerText='Hoàn tất — '+c+' user';
          toast('Sync xong!','#22c55e');
          setTimeout(()=>{ prog.style.width='0%'; },1000);
        }
      });
    },2000);
  }).catch(()=>{
    toast('Sync thất bại — kiểm tra API Key','#ef4444');
    btn.disabled=false; btn.innerHTML='↓ SYNC TẤT CẢ'; prog.style.width='0%';
  });
}

// =========================================
// AS608
// =========================================
function enrollAS() {
  const code = document.getElementById('as_code').value.trim();
  const name = document.getElementById('as_name').value.trim();
  const st   = document.getElementById('as_enroll_status');
  if (!code||!name) { toast('Thiếu mã số hoặc tên','#ef4444'); return; }
  if (!getApiKey()) { toast('Chưa nhập API Key (tab Cài đặt)','#ef4444'); return; }
  st.innerHTML='<span class="spin"></span> Đang enroll — đặt ngón tay lên AS608...';
  fetch('/as608/enroll', {
    method:'POST',
    headers: authHeaders({'Content-Type':'application/x-www-form-urlencoded'}),
    body:'code='+encodeURIComponent(code)+'&name='+encodeURIComponent(name)
  }).then(r=>r.text()).then(d=>{
    st.innerText=d; toast(d, d.includes('thất bại')?'#ef4444':'#22c55e');
    if (!d.includes('thất bại')) {
      document.getElementById('as_code').value='';
      document.getElementById('as_name').value='';
      loadAS608Users();
    }
  }).catch(()=>{st.innerText='Lỗi kết nối';toast('Lỗi','#ef4444');});
}

function syncOneAS() {
  const code = document.getElementById('as_sync_code').value.trim();
  const st   = document.getElementById('as_sync_status');
  if (!code) { toast('Nhập mã số','#ef4444'); return; }
  if (!getApiKey()) { toast('Chưa nhập API Key','#ef4444'); return; }
  st.innerHTML='<span class="spin"></span> Đang sync...';
  fetch('/as608/sync-from-db', {
    method:'POST',
    headers: authHeaders({'Content-Type':'application/json'}),
    body: JSON.stringify({code})
  }).then(r=>r.text()).then(d=>{st.innerText=d;toast(d,'#3b82f6');})
  .catch(()=>{st.innerText='Lỗi';toast('Lỗi','#ef4444');});
}

function syncAllAS() {
  const st = document.getElementById('as_sync_status');
  if (!getApiKey()) { toast('Chưa nhập API Key','#ef4444'); return; }
  st.innerHTML='<span class="spin"></span> Đang sync tất cả...';
  fetch('/as608/sync-from-db', {method:'POST', headers: authHeaders()})
  .then(r=>r.text()).then(d=>{st.innerText=d;toast(d,'#3b82f6');})
  .catch(()=>{st.innerText='Lỗi';toast('Lỗi','#ef4444');});
}

function loadAS608Users() {
  fetch('/as608/users').then(r=>r.json()).then(data=>{
    if (!data.length) {
      document.getElementById('as608_list').innerHTML='<div class="empty">Chưa có user nào</div>';
      return;
    }
    document.getElementById('as608_list').innerHTML=data.map(u=>`
      <div class="item">
        <div class="info"><b>${u.name}</b><span>Mã: ${u.code} · Slot: ${u.slot}</span></div>
        <div class="acts">
          <input class="edit-in" id="as_e_${u.slot}" value="${u.name}">
          <button class="btn update" style="padding:5px 10px;font-size:12px" onclick="updateAS(${u.slot})">Sửa</button>
          <button class="btn delete" style="padding:5px 10px;font-size:12px" onclick="deleteAS(${u.slot})">Xóa</button>
        </div>
      </div>`).join('');
  }).catch(()=>{
    document.getElementById('as608_list').innerHTML='<div class="empty" style="color:#ef4444">Lỗi tải dữ liệu</div>';
  });
}

function updateAS(slot) {
  const name = document.getElementById('as_e_'+slot).value;
  fetch('/as608/update', {
    method:'POST',
    headers: authHeaders({'Content-Type':'application/x-www-form-urlencoded'}),
    body:'slot='+slot+'&name='+encodeURIComponent(name)
  }).then(r=>r.text()).then(d=>{toast(d,'#22c55e');loadAS608Users();});
}

function deleteAS(slot) {
  if (!confirm('Xóa slot '+slot+'?')) return;
  fetch('/as608/delete', {
    method:'POST',
    headers: authHeaders({'Content-Type':'application/x-www-form-urlencoded'}),
    body:'slot='+slot
  }).then(r=>r.text()).then(d=>{toast(d,'#22c55e');loadAS608Users();});
}

// =========================================
// R558S
// =========================================
function enrollRS() {
  const code = document.getElementById('rs_code').value.trim();
  const name = document.getElementById('rs_name').value.trim();
  const st   = document.getElementById('rs_enroll_status');
  if (!code||!name) { toast('Thiếu mã số hoặc tên','#ef4444'); return; }
  if (!getApiKey()) { toast('Chưa nhập API Key (tab Cài đặt)','#ef4444'); return; }
  st.innerHTML='<span class="spin"></span> Đang enroll — đặt ngón tay lên R558S...';
  fetch('/r558s/enroll', {
    method:'POST',
    headers: authHeaders({'Content-Type':'application/x-www-form-urlencoded'}),
    body:'code='+encodeURIComponent(code)+'&name='+encodeURIComponent(name)
  }).then(r=>r.text()).then(d=>{
    st.innerText=d; toast(d, d.includes('thất bại')?'#ef4444':'#22c55e');
    if (!d.includes('thất bại')) {
      document.getElementById('rs_code').value='';
      document.getElementById('rs_name').value='';
      loadR558SUsers();
    }
  }).catch(()=>{st.innerText='Lỗi kết nối';toast('Lỗi','#ef4444');});
}

function syncOneRS() {
  const code = document.getElementById('rs_sync_code').value.trim();
  const st   = document.getElementById('rs_sync_status');
  if (!code) { toast('Nhập mã số','#ef4444'); return; }
  if (!getApiKey()) { toast('Chưa nhập API Key','#ef4444'); return; }
  st.innerHTML='<span class="spin"></span> Đang sync...';
  fetch('/r558s/sync-from-db', {
    method:'POST',
    headers: authHeaders({'Content-Type':'application/json'}),
    body: JSON.stringify({code})
  }).then(r=>r.text()).then(d=>{st.innerText=d;toast(d,'#8b5cf6');})
  .catch(()=>{st.innerText='Lỗi';toast('Lỗi','#ef4444');});
}

function syncAllRS() {
  const st = document.getElementById('rs_sync_status');
  if (!getApiKey()) { toast('Chưa nhập API Key','#ef4444'); return; }
  st.innerHTML='<span class="spin"></span> Đang sync tất cả...';
  fetch('/r558s/sync-from-db', {method:'POST', headers: authHeaders()})
  .then(r=>r.text()).then(d=>{st.innerText=d;toast(d,'#8b5cf6');})
  .catch(()=>{st.innerText='Lỗi';toast('Lỗi','#ef4444');});
}

function loadR558SUsers() {
  fetch('/r558s/users').then(r=>r.json()).then(data=>{
    if (!data.length) {
      document.getElementById('r558s_list').innerHTML='<div class="empty">Chưa có user nào</div>';
      return;
    }
    document.getElementById('r558s_list').innerHTML=data.map(u=>`
      <div class="item">
        <div class="info"><b>${u.name}</b><span>Mã: ${u.code} · Slot: ${u.slot}</span></div>
        <div class="acts">
          <input class="edit-in" id="rs_e_${u.slot}" value="${u.name}">
          <button class="btn update" style="padding:5px 10px;font-size:12px" onclick="updateRS(${u.slot})">Sửa</button>
          <button class="btn delete" style="padding:5px 10px;font-size:12px" onclick="deleteRS(${u.slot})">Xóa</button>
        </div>
      </div>`).join('');
  }).catch(()=>{
    document.getElementById('r558s_list').innerHTML='<div class="empty" style="color:#ef4444">Lỗi tải dữ liệu</div>';
  });
}

function updateRS(slot) {
  const name = document.getElementById('rs_e_'+slot).value;
  fetch('/r558s/update', {
    method:'POST',
    headers: authHeaders({'Content-Type':'application/x-www-form-urlencoded'}),
    body:'slot='+slot+'&name='+encodeURIComponent(name)
  }).then(r=>r.text()).then(d=>{toast(d,'#22c55e');loadR558SUsers();});
}

function deleteRS(slot) {
  if (!confirm('Xóa slot '+slot+'?')) return;
  fetch('/r558s/delete', {
    method:'POST',
    headers: authHeaders({'Content-Type':'application/x-www-form-urlencoded'}),
    body:'slot='+slot
  }).then(r=>r.text()).then(d=>{toast(d,'#22c55e');loadR558SUsers();});
}

// =========================================
// RFID
// =========================================
function scanRFID() {
  const st = document.getElementById('rfid_add_status');
  st.innerHTML='<span class="spin"></span> Đang quét — đưa thẻ lại gần...';
  fetch('/scan-rfid').then(r=>r.text()).then(uid=>{
    if (uid.includes('Timeout')) {
      st.innerText='Timeout — không tìm thấy thẻ'; toast('Không thấy thẻ','#ef4444');
    } else {
      document.getElementById('rfid_uid').value=uid;
      st.innerText='Đã quét: '+uid; toast('Đã quét: '+uid,'#22c55e');
    }
  }).catch(()=>{st.innerText='Lỗi';toast('Lỗi','#ef4444');});
}

function addRFID() {
  const uid  = document.getElementById('rfid_uid').value.trim();
  const name = document.getElementById('rfid_name').value.trim();
  const st   = document.getElementById('rfid_add_status');
  if (!uid||!name) { toast('Thiếu UID hoặc tên','#ef4444'); return; }
  if (!getApiKey()) { toast('Chưa nhập API Key','#ef4444'); return; }
  fetch('/add-rfid', {
    method:'POST',
    headers: authHeaders({'Content-Type':'application/x-www-form-urlencoded'}),
    body:'uid='+encodeURIComponent(uid)+'&name='+encodeURIComponent(name)
  }).then(r=>r.text()).then(d=>{
    st.innerText=d; toast(d,'#22c55e');
    document.getElementById('rfid_uid').value='';
    document.getElementById('rfid_name').value='';
    loadRFIDList();
  });
}

function loadRFIDList() {
  fetch('/rfid-list').then(r=>r.json()).then(data=>{
    if (!data.length) {
      document.getElementById('rfid_list').innerHTML='<div class="empty">Chưa có thẻ nào</div>';
      return;
    }
    document.getElementById('rfid_list').innerHTML=data.map(r=>`
      <div class="item">
        <div class="info"><b>${r.name}</b><span>UID: ${r.uid}</span></div>
        <div class="acts">
          <input class="edit-in" id="rfid_e_${r.id}" value="${r.name}">
          <button class="btn update" style="padding:5px 10px;font-size:12px" onclick="updateRFID(${r.id})">Sửa</button>
          <button class="btn delete" style="padding:5px 10px;font-size:12px" onclick="deleteRFID(${r.id})">Xóa</button>
        </div>
      </div>`).join('');
  }).catch(()=>{
    document.getElementById('rfid_list').innerHTML='<div class="empty" style="color:#ef4444">Lỗi tải dữ liệu</div>';
  });
}

function updateRFID(id) {
  const name = document.getElementById('rfid_e_'+id).value;
  fetch('/rfid-update', {
    method:'POST',
    headers: authHeaders({'Content-Type':'application/x-www-form-urlencoded'}),
    body:'id='+id+'&name='+encodeURIComponent(name)
  }).then(r=>r.text()).then(d=>{toast(d,'#22c55e');loadRFIDList();});
}

function deleteRFID(id) {
  if (!confirm('Xóa thẻ này?')) return;
  fetch('/rfid-delete', {
    method:'POST',
    headers: authHeaders({'Content-Type':'application/x-www-form-urlencoded'}),
    body:'id='+id
  }).then(r=>r.text()).then(d=>{toast(d,'#22c55e');loadRFIDList();});
}

// =========================================
// LOG — lấy từ Node.js server (có đủ RFID, remote)
// =========================================
const logFilterLabels = {
  'as608':'AS608','r558s':'R558S','rfid':'RFID','remote':'Remote'
};
const logMethodBadge = {
  'as608':   '<span class="badge b-blue">AS608</span>',
  'r558s':   '<span class="badge b-purple">R558S</span>',
  'rfid':    '<span class="badge b-cyan">RFID</span>',
  'remote':  '<span class="badge b-orange">Remote</span>',
};

function setLogFilter(f) {
  currentLogFilter = f;
  document.querySelectorAll('.ftab').forEach(btn => {
    btn.classList.toggle('active', btn.innerText.toLowerCase().includes(logFilterLabels[f].toLowerCase()));
  });
  loadLog();
}

function loadLog() {
  // Lấy log từ Node.js server qua proxy /db-users bị trùng — dùng endpoint riêng
  // FIX: đọc từ server Node.js trực tiếp qua proxy mới /api-log
  fetch('/log-proxy?sensor='+currentLogFilter)
  .then(r => {
    if (!r.ok) throw new Error(r.status);
    return r.json();
  })
  .then(data=>{
    const el = document.getElementById('log_list');
    if (!data.length) { el.innerHTML='<div class="empty">Chưa có log nào</div>'; return; }
    el.innerHTML = data.map(l=>{
      const badge = logMethodBadge[l.method] || '';
      return `
        <div class="log-item ${l.granted?'ok':'err'}">
          <div style="font-size:18px">${l.granted?'✅':'❌'}</div>
          <div class="log-info">
            <b>${l.name||'Unknown'} ${badge}</b>
            <span>${l.code?' Mã: '+l.code:''} ${l.uid?' · UID: '+l.uid:''}</span>
          </div>
          <div class="log-time">${l.time||''}</div>
        </div>`;
    }).join('');
  }).catch(()=>{
    document.getElementById('log_list').innerHTML='<div class="empty" style="color:#ef4444">Lỗi tải log — kiểm tra kết nối server</div>';
  });
}

// =========================================
// DATABASE (proxy qua ESP32 → Node.js)
// =========================================
function loadDbUsers() {
  fetch('/db-users').then(r=>r.json()).then(data=>{
    if (!data.length) {
      document.getElementById('db_user_list').innerHTML='<div class="empty">Chưa có user nào trên server</div>';
      return;
    }
    document.getElementById('db_user_list').innerHTML=data.map(u=>{
      const hasFP  = u.fingerprint && u.fingerprint.length > 0;
      const sensors = u.sensors || '—';
      return `
        <div class="item">
          <div class="info">
            <b>${u.name}</b>
            <span>Mã: ${u.code||'—'} · Slot: ${u.slot||'—'} ·
              <span class="badge ${hasFP?'b-green':'b-red'}">${hasFP?'Template ✓':'No template'}</span>
              <span class="badge b-blue">${sensors}</span>
            </span>
          </div>
          <div class="acts">
            <input class="edit-in" id="db_e_${u.code}" value="${u.name}">
            <button class="btn update" style="padding:5px 10px;font-size:12px" onclick="dbUpdateUser('${u.code}')">Sửa</button>
          </div>
        </div>`;
    }).join('');
  }).catch(()=>{
    document.getElementById('db_user_list').innerHTML='<div class="empty" style="color:#ef4444">Không kết nối được server</div>';
  });
}

function loadDbRfid() {
  fetch('/db-rfid').then(r=>r.json()).then(data=>{
    if (!data.length) {
      document.getElementById('db_rfid_list').innerHTML='<div class="empty">Chưa có thẻ RFID nào trên server</div>';
      return;
    }
    document.getElementById('db_rfid_list').innerHTML=data.map(r=>`
      <div class="item">
        <div class="info"><b>${r.name}</b><span>UID: ${r.uid}</span></div>
        <div class="acts">
          <input class="edit-in" id="db_rfid_${r.uid}" value="${r.name}">
          <button class="btn update" style="padding:5px 10px;font-size:12px" onclick="dbUpdateRfid('${r.uid}')">Sửa</button>
        </div>
      </div>`).join('');
  }).catch(()=>{
    document.getElementById('db_rfid_list').innerHTML='<div class="empty" style="color:#ef4444">Không kết nối được server</div>';
  });
}

function dbUpdateUser(code) {
  const name = document.getElementById('db_e_'+code).value;
  if (!getApiKey()) { toast('Chưa nhập API Key','#ef4444'); return; }
  fetch('/db-users', {
    method:'POST',
    headers: authHeaders({'Content-Type':'application/json'}),
    body: JSON.stringify({code, name})
  }).then(r=>r.json()).then(()=>{toast('Đã cập nhật server','#22c55e');loadDbUsers();})
  .catch(()=>toast('Lỗi cập nhật','#ef4444'));
}

function dbUpdateRfid(uid) {
  const name = document.getElementById('db_rfid_'+uid).value;
  if (!getApiKey()) { toast('Chưa nhập API Key','#ef4444'); return; }
  fetch('/db-rfid', {
    method:'POST',
    headers: authHeaders({'Content-Type':'application/json'}),
    body: JSON.stringify({uid, name})
  }).then(r=>r.json()).then(()=>{toast('Đã cập nhật RFID','#22c55e');loadDbRfid();})
  .catch(()=>toast('Lỗi cập nhật','#ef4444'));
}

// =========================================
// SETTINGS
// =========================================
function updateServerIP() {
  const ip = document.getElementById('server_ip_input').value.trim();
  if (!ip) { toast('Nhập IP trước','#ef4444'); return; }
  if (!getApiKey()) { toast('Chưa nhập API Key','#ef4444'); return; }
  fetch('/set-server-ip', {
    method:'POST',
    headers: authHeaders({'Content-Type':'text/plain'}),
    body: ip
  }).then(r=>r.text()).then(d=>{
    document.getElementById('ip_status').innerText = d;
    toast(d,'#22c55e');
  }).catch(()=>toast('Lỗi','#ef4444'));
}

// Khởi tạo
loadStats();
</script>
</body>
</html>
)rawliteral";

#endif