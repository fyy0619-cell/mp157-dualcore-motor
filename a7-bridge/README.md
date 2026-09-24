# A7 桥接(bridge.py)

在 STM32MP157 的 A7/Linux 上运行,连接 M4(RPMsg)与浏览器(WebSocket)。

## 运行

```bash
pip install -r requirements.txt
python3 bridge.py --tty /dev/ttyRPMSG0 --port 8080
# 浏览器打开 http://<板子IP>:8080
```

## 工作方式

- 后台线程持续读 `/dev/ttyRPMSG0`,按行解析 M4 遥测(`T ...`),广播给所有 WebSocket 客户端;
- 浏览器发来的命令(`SPD`/`PID`/`RUN`)经 WebSocket → 写回 `/dev/ttyRPMSG0` 给 M4;
- 通道断开自动重连;静态文件从 `../web` 提供。

## 无硬件时的本地联调(可选)

可用伪终端或普通文件模拟 tty 验证前端:
```bash
# 造一个假 tty,持续写入遥测行,--tty 指过去即可先调 Web/曲线
```
真实运行请在板上、M4 固件已 `remoteproc start` 后进行。
