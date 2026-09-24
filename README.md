# mp157-dualcore-motor

**基于 STM32MP157 异构多核(Cortex-A7 + Cortex-M4)的直流电机实时控制与远程监控系统**

![platform](https://img.shields.io/badge/platform-STM32MP157-informational)
![cores](https://img.shields.io/badge/cores-A7%20Linux%20%2B%20M4%20RT-blue)
![ipc](https://img.shields.io/badge/IPC-OpenAMP%2FRPMsg-orange)
![license](https://img.shields.io/badge/license-MIT-green)

M4 核以 **1 kHz 硬实时**跑 PID 电机闭环,A7 核跑 Linux 提供 **Web 实时控制台**,两核通过
**OpenAMP / RPMsg** 通信。一句话:**又快(M4 确定性控制)又聪明(A7 联网/可视化/AI)**——
这正是通用 SoC 单跑 Linux 做不到、而异构双核才能做到的事。

> 姊妹项目:[`xiangmu`](https://github.com/fyy0619-cell/xiangmu) —— MPU-6050 IIO 内核驱动
> (驱动深度)。本项目 = 系统/BSP + 产品。两者组成"驱动 → 系统 → 产品"作品组合。

## 亮点

- **异构双核 + OpenAMP/RPMsg**:remoteproc 从 Linux 加载/启停 M4 固件,RPMsg 双向通道。
- **硬实时控制**:M4 1 kHz 定时器中断内 编码器测速 → PID(抗积分饱和)→ PWM。
- **客户端友好**:浏览器实时曲线(目标 vs 实测)、拖滑块调速、**在线整定 PID**、一键启停。
- **工程健壮性**:堵转保护、RPMsg 失联进安全态、断线重连、启动顺序管理。
- **可选边缘感知**:挂 MPU-6050 复用 IIO 驱动,把振动/倾角显示到同一面板。

## 演示效果

浏览器里拖动"目标转速"滑块 → 电机实时跟随;调 Kp/Ki/Kd → 阶跃响应肉眼可见变化;
堵住电机 → 面板弹出"堵转保护"并停机。

## 目录结构

```
m4-firmware/     M4 裸机固件(应用逻辑,配合 CubeMX 工程)
  Core/Src|Inc   pid / encoder / motor_pwm / rpmsg_comm / app_control
a7-bridge/       bridge.py(RPMsg↔WebSocket)+ requirements.txt
web/             index.html / app.js / style.css(Chart.js 仪表盘)
scripts/         load_m4.sh(remoteproc)/ check_env.sh(环境自检)
docs/            architecture.md / hardware.md / superpowers/specs
```

## 快速开始(概览)

```bash
# 0) 环境自检(板子 A7/Linux 上)——先确认异构通道可用
sh scripts/check_env.sh

# 1) 在 STM32CubeIDE 里把 m4-firmware/Core 的应用文件集成进 M4 + OpenAMP 工程,编译出 .elf

# 2) 从 Linux 加载并启动 M4 固件
sh scripts/load_m4.sh start motor_ctrl.elf

# 3) 启动 A7 桥接 + Web
cd a7-bridge && pip install -r requirements.txt
python3 bridge.py --tty /dev/ttyRPMSG0 --port 8080

# 4) 浏览器打开 http://<板子IP>:8080
```

## 通信协议(RPMsg 文本帧)

- 下行(A7→M4):`RUN 1|0` · `SPD <rpm>` · `PID <kp> <ki> <kd>`
- 上行(M4→A7):`T <ms> <rpm> <target> <duty> <flags>`(flags bit0 堵转,bit1 运行中)

## 路线图

- [ ] **M0** remoteproc + RPMsg echo 打通(风险验证)
- [ ] **M1** 开环转 + Web 调占空比
- [ ] **M2** 编码器测速 + PID 闭环 + 在线整定
- [ ] **M3** 健壮性(堵转/失联/重连)+ 演示视频
- [ ] **M4(可选)** MPU-6050 振动/倾角集成
- [ ] **未来** BLDC + FOC 矢量控制(名头升级)

## 状态

代码/架构/文档已就绪;硬件上板与联调按里程碑推进中(详见
`docs/superpowers/specs/`)。M4 侧 HAL 初始化由 CubeMX 生成,本仓库提供应用逻辑文件。

## 许可

MIT,见 [LICENSE](LICENSE)。学习 / 作品集用途。
