#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""A7 bridge: RPMsg <-> WebSocket + static web server.

Reads M4 telemetry from the RPMsg tty (/dev/ttyRPMSG0) and pushes it to any
connected browser over WebSocket; forwards browser commands back to the M4.

Run on the STM32MP157 (A7/Linux) after the M4 firmware is loaded:
    python3 bridge.py --tty /dev/ttyRPMSG0 --port 8080
Then open http://<board-ip>:8080 in a browser on the same network.
"""
import argparse
import threading
import time

from flask import Flask, send_from_directory
from flask_sock import Sock

app = Flask(__name__, static_folder=None)
sock = Sock(app)

WEB_DIR = "../web"
_clients = set()
_clients_lock = threading.Lock()
_tty = None
_tty_lock = threading.Lock()


def _broadcast(text):
    dead = []
    with _clients_lock:
        for ws in _clients:
            try:
                ws.send(text)
            except Exception:
                dead.append(ws)
        for ws in dead:
            _clients.discard(ws)


def _tty_reader(path):
    """Reopen-on-failure reader that forwards M4 telemetry lines to clients."""
    global _tty
    while True:
        try:
            with open(path, "r+b", buffering=0) as f:
                with _tty_lock:
                    _tty = f
                buf = b""
                while True:
                    chunk = f.read(64)
                    if not chunk:
                        time.sleep(0.005)
                        continue
                    buf += chunk
                    while b"\n" in buf:
                        line, buf = buf.split(b"\n", 1)
                        s = line.decode("ascii", "ignore").strip()
                        if s:
                            _broadcast(s)
        except Exception as e:
            with _tty_lock:
                _tty = None
            print("tty reader: %s (retrying)" % e)
            time.sleep(1.0)


def _send_to_m4(line):
    data = (line.strip() + "\n").encode("ascii")
    with _tty_lock:
        if _tty is not None:
            try:
                _tty.write(data)
            except Exception as e:
                print("write to M4 failed: %s" % e)


@app.route("/")
def index():
    return send_from_directory(WEB_DIR, "index.html")


@app.route("/<path:path>")
def static_files(path):
    return send_from_directory(WEB_DIR, path)


@sock.route("/ws")
def ws_route(ws):
    with _clients_lock:
        _clients.add(ws)
    try:
        while True:
            msg = ws.receive()          # e.g. "SPD 1200", "PID 0.5 0.1 0.01", "RUN 1"
            if msg is None:
                break
            _send_to_m4(msg)
    finally:
        with _clients_lock:
            _clients.discard(ws)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--tty", default="/dev/ttyRPMSG0")
    ap.add_argument("--port", type=int, default=8080)
    args = ap.parse_args()

    threading.Thread(target=_tty_reader, args=(args.tty,), daemon=True).start()
    print("bridge on :%d  tty=%s" % (args.port, args.tty))
    app.run(host="0.0.0.0", port=args.port, threaded=True)


if __name__ == "__main__":
    main()
