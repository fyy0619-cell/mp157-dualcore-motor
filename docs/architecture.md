# 架构说明

## 为什么是异构双核(项目灵魂)

STM32MP157 同时集成 **Cortex-A7(跑 Linux)** 与 **Cortex-M4(跑裸机实时)**。本项目
刻意利用这一点:

- **M4 = 硬实时控制大脑**:PID 速度闭环对时序抖动敏感,必须在确定性上下文里以固定
  1 kHz 执行。裸机 + 定时器中断可做到微秒级确定性。
- **A7 = 智能与连接**:Linux 抢占式调度**无法保证硬实时**,但擅长网络、Web、存储、AI。
  它负责把 M4 的数据可视化、接收人机指令、(可选)跑边缘 AI。
- **OpenAMP / RPMsg = 两核之间的桥**:remoteproc 从 Linux 加载/启停 M4 固件,RPMsg 提供
  双向消息通道(在 A7 侧表现为 `/dev/ttyRPMSG0`)。

> 面试论点:把 PID 放到 A7 会因调度抖动导致控制不稳——这正是"为什么需要异构双核"的
> 教科书答案。

## 分层框图

```
        浏览器(Chart.js 实时曲线 + PID 滑块 + 启停)
                     │  WebSocket
     ┌───────────────┴─────────────────────────────┐
     │  A7 / Linux                                  │
     │   bridge.py (Flask + flask-sock)             │
     │     ├─ 读 /dev/ttyRPMSG0 遥测 → 推浏览器      │
     │     └─ 浏览器命令 → 写 /dev/ttyRPMSG0         │
     │   remoteproc(加载/启停 M4 固件)              │
     │   (可选)IIO 读 MPU6050 → 面板                │
     └───────────────┬─────────────────────────────┘
                     │  RPMsg(OpenAMP)
     ┌───────────────┴─────────────────────────────┐
     │  M4 / 裸机(硬实时)                          │
     │   1 kHz 控制中断:                            │
     │     encoder_update → pid_update → motor_pwm  │
     │   50 Hz 遥测上行;堵转/失联保护               │
     └───────────────┬─────────────────────────────┘
                     │  PWM + 方向 / 编码器
              TB6612 H 桥 ── 直流电机(+编码器)
```

## 三条数据流

1. **控制环(M4,1 kHz,实时)**:编码器计数 → 测速 → PID → PWM 占空比。全程不出 M4。
2. **遥测上行(~50 Hz)**:M4 `T <ms> <rpm> <target> <duty> <flags>` → RPMsg → ttyRPMSG0
   → Python → WebSocket → 浏览器曲线。
3. **命令下行**:滑块/按钮 → WebSocket → Python → ttyRPMSG0 → M4 更新目标/PID/启停。

## 模块与职责

| 模块 | 文件 | 职责 | 可独立测试 |
|------|------|------|------------|
| PID | `pid.c/.h` | 控制律 + 抗积分饱和 | ✅ 纯函数,主机可测 |
| 测速 | `encoder.c/.h` | 计数差 → rpm,处理回绕 | ✅ 纯函数 |
| 驱动 | `motor_pwm.c/.h` | 有符号占空比 → PWM+方向 | 依赖 HAL |
| 协议 | `rpmsg_comm.c/.h` | 文本帧解析/格式化 + 传输钩子 | ✅ 解析/格式化可测 |
| 调度 | `app_control.c` | 1 kHz 中断 + 遥测 + 保护 | 集成层 |
| 桥接 | `bridge.py` | RPMsg ↔ WebSocket + 静态服务 | 主机可跑(接伪 tty) |
| 前端 | `web/` | 实时曲线 + 控件 | 浏览器 |

## 里程碑

M0 打通 remoteproc+RPMsg echo → M1 开环转+Web 调占空比 → M2 编码器+PID 闭环 →
M3 健壮性+演示 → M4(可选)IIO 集成。详见 `docs/superpowers/specs/`。
