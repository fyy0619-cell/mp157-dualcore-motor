# 基于 STM32MP157 异构多核的直流电机实时控制与远程监控系统 设计文档

日期:2026-09-24
作者:fyy0619-cell
状态:已确认,待实现

## 1. 背景与目标

作者研二,方向嵌入式 AI / TinyML,目标 2027 年 3-4 月冲击嵌入式驱动 / BSP 方向实习。
本项目作为**旗舰作品**,展示对 STM32MP157 **异构多核(Cortex-A7 + Cortex-M4)**架构的掌握:
用 M4 做硬实时电机控制,A7 跑 Linux 做联网、可视化与(可选)边缘 AI,两核经 **OpenAMP / RPMsg**
通信。它同时具备"底层硬功夫(BSP/实时/双核通信)"与"产品客户端(Web 仪表盘)",可现场演示。

与作者的 IIO 驱动项目(仓库 `xiangmu`)互补:IIO = 驱动深度;本项目 = 系统/BSP + 产品。
本项目**轻度集成** IIO:可选把 MPU6050 装在电机平台上,复用 IIO 驱动采振动/倾角显示到同一面板。

### 为什么必须双核(项目灵魂)
PID 闭环对时序抖动敏感,必须放 **M4 做毫秒级确定性执行**;Linux 抢占式调度无法保证硬实时,
只负责它擅长的网络、界面、存储与 AI。把控制环放 A7 就失去了本项目的意义。这是面试核心论点。

### 非目标(YAGNI)
- 第一版**不做 BLDC FOC**(电流环、Clarke/Park、SVPWM 风险高);用有刷直流 + 编码器 + PID。
  FOC 列入"未来工作",名头保留。
- 不做云端/App;客户端就是浏览器 Web 面板(局域网访问板子)。

## 2. 硬件平台与清单

- 主控:100ASK STM32MP157 PRO(A7 跑 Linux,M4 跑裸机固件)。
- 电机:带**正交编码器**的直流减速电机(~¥30–60)。
- 驱动:H 桥 **TB6612**(推荐,优于 L298N)。
- 供电:电机独立供电,与板**共地**。
- 可选:MPU6050(已有,IIO 驱动复用)。
- 联调:串口 CH340 @115200 看 dmesg;板与 PC/VM 同一局域网访问 Web。

## 3. 架构分层

1. **M4 核(裸机,硬实时)——控制大脑**
   - 定时器**编码器模式**硬件解码正交编码器;
   - 1 kHz 控制中断:测速 → **PID 速度闭环** → 定时器 PWM 经 TB6612 驱动电机;
   - 经 RPMsg 收"目标转速 / PID 参数 / 启停",周期回传遥测。
2. **A7 核(Linux)——桥接 + 服务**
   - RPMsg 通道暴露为 `/dev/ttyRPMSG0`;
   - **Python 桥接**(Flask + flask-sock):读遥测 → WebSocket 推浏览器;收命令 → 写通道给 M4;
   - **remoteproc**:用户空间加载/启停 M4 固件;
   - **Web 仪表盘**:实时转速曲线(目标 vs 实际)、调速滑块、在线整定 PID、启停;
   - **(可选)IIO**:读 MPU6050 振动/倾角并入面板。

## 4. 通信协议(RPMsg 上的文本帧)

- 下行(A7→M4):`RUN 1|0`、`SPD <rpm>`、`PID <kp> <ki> <kd>`。
- 上行(M4→A7)遥测(~50 Hz):`T <ms> <rpm> <target> <duty> <flags>`
  - flags:bit0 堵转、bit1 运行中。
- 文本协议便于调试(可直接 `cat /dev/ttyRPMSG0`);后续可升级为二进制结构体。

## 5. 数据流(三条)

- **控制环(M4,1 kHz,实时)**:编码器 → 测速 → PID → PWM,闭环全程不出 M4。
- **遥测上行(~50 Hz)**:M4 → RPMsg → `/dev/ttyRPMSG0` → Python → WebSocket → 浏览器曲线。
- **命令下行**:滑块 → WebSocket → Python → 通道 → M4 更新目标/参数。

## 6. 组件划分(单一职责)

- M4 固件:`encoder`(测速)、`motor_pwm`(PWM+方向)、`pid`(控制律)、`rpmsg_comm`(协议)、`main`(调度)。
- A7 桥接(Python):`bridge.py`(rpmsg_io + ws_server + 静态服务)。
- Web 前端:`index.html` / `app.js`(Chart.js + 控件)/ `style.css`。
- 脚本:`load_m4.sh`(remoteproc 加载/启停)、`check_env.sh`(环境自检)。

## 7. 错误处理

- M4:**堵转检测**(高占空比却近零转速→停机上报)、看门狗、PID 输出限幅 + 抗积分饱和、
  **RPMsg 失联进安全态(停机)**。
- A7:通道断开重连、M4 崩溃经 remoteproc 恢复、WebSocket 断线重连;启动顺序(先加载 M4 再开桥接)。

## 8. 里程碑

- **M0 打通异构通道**:remoteproc 加载 M4 固件 + RPMsg echo(先证明板子能通,风险验证)。
- **M1 开环转 + Web 调占空比**:端到端最小系统(电机开环)。
- **M2 编码器测速 + PID 闭环**:M4 闭环,Web 显示目标/实际曲线、在线调 PID。
- **M3 健壮性 + 演示打磨**:堵转/失联保护、启动顺序、断线重连、演示视频。
- **M4(可选)IIO 集成**:MPU6050 振动/倾角挂同一面板。

## 9. 测试与证据

- 台架 PID 阶跃响应;Web 拖滑块→转速实时变化;改 PID 明显改变响应;堵转保护触发。
- **证据**:转速曲线截图、示波器抓 PWM、`/proc/interrupts` 中断计数、演示视频。

## 10. 仓库结构

```
mp157-dualcore-motor/
  m4-firmware/     Core/Src, Core/Inc(encoder/motor_pwm/pid/rpmsg_comm/main)
  a7-bridge/       bridge.py  requirements.txt
  web/             index.html  app.js  style.css
  scripts/         load_m4.sh  check_env.sh
  docs/            architecture.md  hardware.md  superpowers/specs/...
```

## 11. 已知边界(诚实说明)

- M4 侧 HAL 初始化由 STM32CubeMX 生成;仓库内 `Core/Src` 提供**应用逻辑**文件,需与 CubeMX 工程集成。
- remoteproc/RPMsg 能否直接工作取决于板子 Linux BSP 配置(`CONFIG_REMOTEPROC`、STM32 remoteproc、rpmsg tty),
  M0 里先验证/开启。
- 第一版有刷 + PID;BLDC FOC 为未来工作。
