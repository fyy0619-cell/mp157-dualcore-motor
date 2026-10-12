#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Step B 电机验证 bridge(零依赖,仅标准库)。
浏览器 <-(SSE/HTTP)-> 本程序(VM) <-(ssh)-> 板子 /dev/ttyRPMSG0 <-(RPMsg)-> M4(PID+模拟电机)

上行:M4 的 "T ms rpm target dutyPct flags" 遥测 -> SSE 推给浏览器画曲线
下行:浏览器 RUN/SPD/PID 命令 -> ssh 写进 ttyRPMSG0 -> M4
跑在 VM:python3 bridge_motor.py   浏览器开 http://<VM-ip>:8080
"""
import http.server
import socketserver
import subprocess
import threading
import queue
import time
import urllib.parse

BOARD = "192.168.2.250"
TTY = "/dev/ttyRPMSG0"
PORT = 8080
SSH = ["ssh", "-o", "StrictHostKeyChecking=no", "-o", "BatchMode=yes",
       "-o", "ConnectTimeout=5", "root@" + BOARD]

clients = []
clients_lock = threading.Lock()


def broadcast(line):
    with clients_lock:
        for q in list(clients):
            try:
                q.put_nowait(line)
            except Exception:
                pass


def reader_thread():
    while True:
        try:
            p = subprocess.Popen(SSH + ["tail -n 1 -f /tmp/tele.txt"],
                                 stdout=subprocess.PIPE,
                                 stdin=subprocess.DEVNULL, bufsize=0)
            print("[reader] ssh cat started pid=%s" % p.pid, flush=True)
            for raw in iter(p.stdout.readline, b""):
                line = raw.decode("ascii", "ignore").strip()
                if line:
                    broadcast(line)
            print("[reader] ssh cat ended rc=%s" % p.poll(), flush=True)
        except Exception as e:
            print("[reader] error: %s" % e, flush=True)
        time.sleep(0.15)   # 快速重连,避免 M4 在无读者空窗里灌满缓冲而阻塞


def send_cmd(cmd):
    # 只允许 RUN/SPD/PID + 数字空格,防注入
    if not cmd or cmd.split()[0] not in ("RUN", "SPD", "PID"):
        return False
    if any(ch not in "RUNSPDI 0123456789-" for ch in cmd):
        return False
    remote = "printf '%s\\n' > %s" % (cmd, TTY)
    try:
        subprocess.run(SSH + [remote], timeout=8)
        return True
    except Exception:
        return False


PAGE = """<!DOCTYPE html><html lang="zh-CN"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>MP157 双核 · 电机实时控制台(模拟电机)</title>
<script src="https://cdn.jsdelivr.net/npm/chart.js@4"></script>
<style>
 body{font-family:"Microsoft YaHei",system-ui,sans-serif;background:#f4f7fc;color:#232a33;margin:0;padding:20px;}
 h1{color:#0b3d91;font-size:20px;margin:0 0 4px;}
 .sub{color:#667085;font-size:13px;margin-bottom:14px;}
 #status{display:inline-block;padding:2px 10px;border-radius:12px;font-size:12px;color:#fff;}
 .ok{background:#1e8449;} .bad{background:#c0392b;}
 .wrap{display:flex;gap:18px;flex-wrap:wrap;}
 .card{background:#fff;border:1px solid #e1e8f2;border-radius:12px;padding:18px;box-shadow:0 2px 10px rgba(11,61,145,.06);}
 .chart{flex:1 1 560px;height:340px;} canvas{width:100%!important;height:300px!important;}
 .panel{flex:0 0 300px;}
 button{font-size:15px;padding:10px 20px;border:none;border-radius:8px;cursor:pointer;color:#fff;background:#1e8449;}
 button.stop{background:#c0392b;}
 label{display:block;margin:14px 0 4px;font-size:14px;}
 input[type=range]{width:100%;}
 .readout{margin:10px 0;font-size:15px;}
 .readout b{color:#0b3d91;font-size:18px;}
 .stall{color:#c0392b;font-weight:bold;}
 fieldset{border:1px solid #dce4f0;border-radius:8px;margin-top:14px;}
 legend{color:#0b3d91;font-size:13px;}
 output{color:#c0392b;font-weight:bold;}
</style></head><body>
<h1>STM32MP157 异构双核 · 电机实时控制台</h1>
<div class="sub">M4(1kHz PID + 模拟电机)↔ RPMsg ↔ A7 bridge ↔ 浏览器 · 连接 <span id="status" class="bad">未连接</span></div>
<div class="wrap">
 <div class="card chart"><canvas id="chart"></canvas></div>
 <div class="card panel">
   <button id="btnRun" onclick="toggleRun()">启动</button>
   <div class="readout">实测 <b id="rpm">0</b> rpm · 占空比 <b id="duty">0.00</b></div>
   <label>目标转速 <output id="spdOut">0</output> rpm
     <input type="range" id="spd" min="0" max="3000" value="0" step="10" oninput="onSpd()"></label>
   <fieldset><legend>PID 在线整定</legend>
     <label>Kp <output id="kpO">0.0008</output>
       <input type="range" id="kp" min="0" max="0.005" value="0.0008" step="0.0001" oninput="onPid()"></label>
     <label>Ki <output id="kiO">0.0040</output>
       <input type="range" id="ki" min="0" max="0.02" value="0.0040" step="0.0001" oninput="onPid()"></label>
     <label>Kd <output id="kdO">0.00002</output>
       <input type="range" id="kd" min="0" max="0.0002" value="0.00002" step="0.00001" oninput="onPid()"></label>
   </fieldset>
 </div>
</div>
<script>
 const chart=new Chart(document.getElementById('chart'),{type:'line',
   data:{labels:[],datasets:[
     {label:'目标 rpm',data:[],borderColor:'#c0392b',borderDash:[6,4],pointRadius:0},
     {label:'实测 rpm',data:[],borderColor:'#0b5cff',pointRadius:0,tension:.15}]},
   options:{animation:false,responsive:true,maintainAspectRatio:false,
     scales:{x:{display:false},y:{beginAtZero:true,suggestedMax:3000,title:{display:true,text:'rpm'}}}}});
 const MAX=300;
 function push(t,r){const d=chart.data;d.labels.push('');d.datasets[0].data.push(t);d.datasets[1].data.push(r);
   if(d.labels.length>MAX){d.labels.shift();d.datasets[0].data.shift();d.datasets[1].data.shift();}chart.update('none');}
 const st=document.getElementById('status');
 const es=new EventSource('/events');
 es.onopen=()=>{st.textContent='已连接';st.className='ok';};
 es.onerror=()=>{st.textContent='断开(重连中)';st.className='bad';};
 es.onmessage=(e)=>{const p=e.data.split(/\\s+/);
   if(p[0]!=='T'||p.length<6)return;
   const rpm=+p[2],target=+p[3],duty=(+p[4])/100;
   document.getElementById('rpm').textContent=rpm;
   document.getElementById('duty').textContent=duty.toFixed(2);
   push(target,rpm);};
 function cmd(c){fetch('/cmd?c='+encodeURIComponent(c));}
 let running=false;
 function toggleRun(){running=!running;cmd('RUN '+(running?1:0));
   const b=document.getElementById('btnRun');b.textContent=running?'停止':'启动';b.className=running?'stop':'';}
 function onSpd(){const v=document.getElementById('spd').value;document.getElementById('spdOut').textContent=v;cmd('SPD '+v);}
 function onPid(){const kp=+document.getElementById('kp').value,ki=+document.getElementById('ki').value,kd=+document.getElementById('kd').value;
   document.getElementById('kpO').textContent=kp.toFixed(4);
   document.getElementById('kiO').textContent=ki.toFixed(4);
   document.getElementById('kdO').textContent=kd.toFixed(5);
   cmd('PID '+Math.round(kp*1e6)+' '+Math.round(ki*1e6)+' '+Math.round(kd*1e6));}
</script></body></html>"""


class Handler(http.server.BaseHTTPRequestHandler):
    def log_message(self, *a):
        pass

    def do_GET(self):
        u = urllib.parse.urlparse(self.path)
        if u.path == "/":
            body = PAGE.encode("utf-8")
            self.send_response(200)
            self.send_header("Content-Type", "text/html; charset=utf-8")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)
        elif u.path == "/cmd":
            qs = urllib.parse.parse_qs(u.query)
            c = (qs.get("c") or [""])[0]
            ok = send_cmd(c)
            self.send_response(200 if ok else 400)
            self.end_headers()
            self.wfile.write(b"ok" if ok else b"bad")
        elif u.path == "/events":
            self.send_response(200)
            self.send_header("Content-Type", "text/event-stream")
            self.send_header("Cache-Control", "no-cache")
            self.end_headers()
            q = queue.Queue(maxsize=500)
            with clients_lock:
                clients.append(q)
            try:
                while True:
                    try:
                        line = q.get(timeout=15)
                        self.wfile.write(("data: %s\n\n" % line).encode("utf-8"))
                    except queue.Empty:
                        self.wfile.write(b": ka\n\n")
                    self.wfile.flush()
            except Exception:
                pass
            finally:
                with clients_lock:
                    if q in clients:
                        clients.remove(q)
        else:
            self.send_response(404)
            self.end_headers()


class Server(socketserver.ThreadingMixIn, http.server.HTTPServer):
    daemon_threads = True


if __name__ == "__main__":
    threading.Thread(target=reader_thread, daemon=True).start()
    print("motor bridge on :%d board=%s tty=%s" % (PORT, BOARD, TTY))
    Server(("0.0.0.0", PORT), Handler).serve_forever()
