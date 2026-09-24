# M4 固件(应用逻辑)

本目录是 **应用逻辑**,需集成进一个 STM32CubeIDE 生成的 STM32MP157 **M4 + OpenAMP** 工程。

## 集成步骤

1. 用 STM32CubeMX 为 STM32MP157 的 **Cortex-M4** 新建工程,启用:
   - 一个定时器 **PWM** 输出(电机调速)+ 两个 GPIO(TB6612 AIN1/AIN2 方向);
   - 一个定时器 **Encoder Mode**(编码器 A/B);
   - 一个定时器产生 **1 kHz 中断**(控制节拍);
   - **OpenAMP** 中间件(RESGROUP / RPMsg / VIRT_UART)。
2. 把本目录 `Core/Src` 与 `Core/Inc` 的文件加入工程:
   `pid.*`、`encoder.*`、`motor_pwm.*`、`rpmsg_comm.*`、`app_control.c`。
3. 按 `// TODO(CubeMX)` / `// TODO(OpenAMP)` 注释填入你的 HAL 句柄:
   - `motor_pwm.c`:PWM 定时器句柄、AIN1/AIN2 GPIO;
   - `app_control.c`:实现 `board_read_encoder_count()`(读编码器定时器 CNT);
   - 在 1 kHz 定时器中断回调里调用 `app_control_tick()`;主循环调用 `app_control_poll()`;
   - `rpmsg_comm.c`:把 `rpmsg_send_line()` 接到 VIRT_UART/RPMsg 发送,RX 回调里
     `rpmsg_parse_line()` → `app_control_apply()`。

## 可主机测试的纯逻辑

`pid.c`、`encoder.c`、`rpmsg_comm.c` 的解析/格式化不依赖 HAL,可在 PC 上写单元测试
(阶跃响应、计数回绕、协议往返)。

## 参数

- `ENCODER_CPR`(`app_control.c`):按电机 = 编码器每转脉冲 × 4(x4 解码) × 减速比 设置。
- 起始 PID 增益为保守值,最终在 Web 面板在线整定。
