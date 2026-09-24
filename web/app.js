// SPDX-License-Identifier: MIT
// Live dashboard: WebSocket <-> A7 bridge <-> M4 control loop.

const MAXPTS = 300; // ~6 s at 50 Hz
const chart = new Chart(document.getElementById('chart'), {
  type: 'line',
  data: {
    labels: [],
    datasets: [
      { label: '目标 rpm', data: [], borderColor: '#c0392b', borderDash: [6, 4], pointRadius: 0, tension: 0 },
      { label: '实测 rpm', data: [], borderColor: '#0b5cff', pointRadius: 0, tension: 0.15 },
    ],
  },
  options: {
    animation: false, responsive: true, maintainAspectRatio: false,
    scales: { x: { display: false }, y: { beginAtZero: true, title: { display: true, text: 'rpm' } } },
  },
});

function pushPoint(target, rpm) {
  const d = chart.data;
  d.labels.push('');
  d.datasets[0].data.push(target);
  d.datasets[1].data.push(rpm);
  if (d.labels.length > MAXPTS) {
    d.labels.shift(); d.datasets[0].data.shift(); d.datasets[1].data.shift();
  }
  chart.update('none');
}

// ---- WebSocket with auto-reconnect ----
let ws = null;
function connect() {
  ws = new WebSocket(`ws://${location.host}/ws`);
  ws.onopen = () => setStatus(true);
  ws.onclose = () => { setStatus(false); setTimeout(connect, 1000); };
  ws.onmessage = (ev) => onTelemetry(ev.data);
}
function send(line) { if (ws && ws.readyState === 1) ws.send(line); }

function onTelemetry(s) {
  // "T <ms> <rpm> <target> <duty> <flags>"
  const p = s.split(/\s+/);
  if (p[0] !== 'T' || p.length < 6) return;
  const rpm = parseFloat(p[2]), target = parseFloat(p[3]), duty = parseFloat(p[4]);
  const flags = parseInt(p[5], 10);
  document.getElementById('rpm').textContent = rpm.toFixed(0);
  document.getElementById('duty').textContent = duty.toFixed(2);
  document.getElementById('stall').classList.toggle('hidden', !(flags & 0x1));
  pushPoint(target, rpm);
}

function setStatus(ok) {
  const b = document.getElementById('status');
  b.textContent = ok ? '已连接' : '未连接';
  b.className = 'badge ' + (ok ? 'on' : 'off');
}

// ---- controls ----
let running = false;
const btnRun = document.getElementById('btnRun');
btnRun.onclick = () => {
  running = !running;
  send('RUN ' + (running ? 1 : 0));
  btnRun.textContent = running ? '停止' : '启动';
  btnRun.classList.toggle('stopbtn', running);
};

const spd = document.getElementById('spd');
spd.oninput = () => {
  document.getElementById('spdVal').textContent = spd.value;
  send('SPD ' + spd.value);
};

function bindOut(id) {
  const el = document.getElementById(id);
  el.oninput = () => document.getElementById(id + 'Val').textContent = el.value;
}
['kp', 'ki', 'kd'].forEach(bindOut);
document.getElementById('btnPid').onclick = () => {
  send(`PID ${document.getElementById('kp').value} ${document.getElementById('ki').value} ${document.getElementById('kd').value}`);
};

connect();
