# STM32MP157 双核电机控制系统 实现计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在 STM32MP157 上实现 M4 硬实时 PID 直流电机闭环 + A7 Linux Web 远程控制台,两核经 OpenAMP/RPMsg 通信。

**Architecture:** M4 裸机在 1 kHz 中断内跑 编码器→PID→PWM;A7 用 Python 桥接把 `/dev/ttyRPMSG0` 的遥测经 WebSocket 推给浏览器,并把浏览器命令写回;remoteproc 从 Linux 加载 M4 固件。

**Tech Stack:** STM32CubeIDE(M4)、OpenAMP/RPMsg、remoteproc、Python(Flask + flask-sock)、Chart.js、gcc(主机单元测试)。

## Global Constraints

- 目标板:100ASK STM32MP157 PRO;M4 裸机固件,A7 跑 Linux。
- 控制频率 1 kHz(`CONTROL_DT = 0.001`);遥测 50 Hz(`TELEMETRY_DECIM = 20`)。
- RPMsg 协议(文本):下行 `RUN 1|0` / `SPD <rpm>` / `PID <kp> <ki> <kd>`;上行 `T <ms> <rpm> <target> <duty> <flags>`,flags bit0 堵转、bit1 运行中。
- 已有函数签名(不得改名):`pid_init/pid_set_gains/pid_reset/pid_update`、`encoder_init/encoder_update`、`rpmsg_parse_line/rpmsg_format_telemetry`、`app_control_init/apply/tick/poll`、`board_read_encoder_count`。
- A7 桥接:`bridge.py --tty /dev/ttyRPMSG0 --port 8080`;Web 静态目录 `../web`。
- 联调:串口 CH340 @115200 看 dmesg;板与 PC 同网访问 Web。
- 电机独立供电 + 与板共地;首次上电目标转速 0、限幅。

## 文件结构(新增)

```
tests/                      主机单元测试(纯逻辑,gcc)
  test_pid.c  test_encoder.c  test_protocol.c  run_tests.sh
m4-firmware/Core/...         已存在:pid/encoder/motor_pwm/rpmsg_comm/app_control
a7-bridge/bridge.py          已存在
web/                         已存在
scripts/                     已存在:load_m4.sh / check_env.sh
docs/evidence/               上板证据截图(后续补)
```

---

## Task 0(M0):打通 remoteproc + RPMsg echo(风险验证)

**Files:**
- 使用:`scripts/check_env.sh`、`scripts/load_m4.sh`
- 产出:一个最小 M4 echo 固件(STM32CubeIDE 工程,不入本仓库源码树)

**Interfaces:**
- Produces:板上出现 `/dev/ttyRPMSG0`,A7 写入一行、M4 原样回显。

- [ ] **Step 1: 板上环境自检**

在板子(A7/Linux)上运行:
```sh
sh scripts/check_env.sh
```
Expected:`/sys/class/remoteproc/remoteproc0 present`;能看到 REMOTEPROC/RPMSG 的 config 行。
若 MISSING → 需在内核开 `CONFIG_REMOTEPROC`、STM32 remoteproc、`CONFIG_RPMSG_TTY`(先解决再继续)。

- [ ] **Step 2: 用 CubeMX 生成 M4 OpenAMP echo 工程**

STM32CubeMX:选 STM32MP157 的 Cortex-M4 → 启用 OpenAMP(RESMGR_TABLE + RPMsg)→ 用 ST 提供的 `OpenAMP_TTY_echo` 示例作模板,生成并在 STM32CubeIDE 编译出 `.elf`。

- [ ] **Step 3: 从 Linux 加载并启动 M4 固件**

```sh
sh scripts/load_m4.sh start <你的echo固件>.elf
sh scripts/load_m4.sh status
```
Expected:`state=running`;`/dev/ttyRPMSG0` 出现。

- [ ] **Step 4: 验证双向 echo**

```sh
# 终端 A 读:
cat /dev/ttyRPMSG0 &
# 终端 B 写:
printf 'hello\n' > /dev/ttyRPMSG0
```
Expected:终端 A 打印回显 `hello`(证明 A7↔M4 通道打通)。

- [ ] **Step 5: 记录证据 + 提交**

保存 dmesg / 命令输出截图到 `docs/evidence/m0-rpmsg.txt`(粘贴文本亦可)。
```bash
git add docs/evidence/m0-rpmsg.txt
git commit -m "M0: verify remoteproc + RPMsg echo channel on board"
```

---

## Task 1:纯逻辑主机单元测试(TDD,不需硬件)

**Files:**
- Create: `tests/test_pid.c`, `tests/test_encoder.c`, `tests/test_protocol.c`, `tests/run_tests.sh`
- 使用:`m4-firmware/Core/Src/{pid,encoder,rpmsg_comm}.c` + `Core/Inc`

**Interfaces:**
- Consumes:`pid_update`、`encoder_update`、`rpmsg_parse_line`、`rpmsg_format_telemetry`。
- Produces:一条 `tests/run_tests.sh`,全绿即纯逻辑正确。

- [ ] **Step 1: 写 PID 收敛测试(先失败)**

`tests/test_pid.c`:
```c
#include "pid.h"
#include <assert.h>
#include <stdio.h>
#include <math.h>

/* 一阶被控对象模拟:v' = (-v + k*u)/tau,验证 PID 能把 v 拉到设定值 */
int main(void) {
    pid_t p; pid_init(&p, 0.02f, 0.5f, 0.0f, 0.001f, -1.0f, 1.0f);
    float v = 0.0f, setpoint = 100.0f, k = 200.0f, tau = 0.05f, dt = 0.001f;
    for (int i = 0; i < 5000; i++) {
        float u = pid_update(&p, setpoint, v);
        v += ( -v + k*u ) * dt / tau;
    }
    printf("final v=%.2f\n", v);
    assert(fabsf(v - setpoint) < 2.0f);      /* 稳态误差 < 2 */
    /* 输出限幅 */
    pid_t q; pid_init(&q, 100.0f, 0, 0, 0.001f, -1.0f, 1.0f);
    assert(pid_update(&q, 1000.0f, 0.0f) <= 1.0f);
    printf("PID OK\n");
    return 0;
}
```

- [ ] **Step 2: 运行,确认(未编译)失败**

Run: `cc -I m4-firmware/Core/Inc tests/test_pid.c m4-firmware/Core/Src/pid.c -lm -o /tmp/tp && /tmp/tp`
Expected:此刻应能编过并 PASS(pid.c 已实现)。若断言失败,调 `pid_init` 增益后重试——**这一步的意义是把"PID 真能收敛"变成可复现的证据**。

- [ ] **Step 3: 写编码器回绕测试**

`tests/test_encoder.c`:
```c
#include "encoder.h"
#include <assert.h>
#include <stdio.h>
#include <math.h>

int main(void) {
    encoder_t e; encoder_init(&e, 1000, 0.001f);   /* 1000 cpr, 1ms */
    encoder_update(&e, 0);
    float rpm = encoder_update(&e, 100);            /* +100 counts in 1ms */
    /* 100/1000 rev in 1ms => 0.1*60000 = 6000 rpm */
    assert(fabsf(rpm - 6000.0f) < 1.0f);
    /* 16-bit 回绕:65500 -> 20 应为 +56 counts,不是 -65480 */
    encoder_init(&e, 1000, 0.001f);
    encoder_update(&e, 65500);
    rpm = encoder_update(&e, 20);
    assert(rpm > 0);
    printf("ENCODER OK\n");
    return 0;
}
```

- [ ] **Step 4: 写协议往返测试**

`tests/test_protocol.c`:
```c
#include "rpmsg_comm.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>

int main(void) {
    motor_cmd_t c;
    assert(rpmsg_parse_line("SPD 1200", &c) && c.have_spd && c.spd == 1200.0f);
    assert(rpmsg_parse_line("RUN 1", &c) && c.have_run && c.run == 1.0f);
    assert(rpmsg_parse_line("PID 0.5 0.1 0.01", &c) && c.have_pid && c.kp == 0.5f);
    assert(!rpmsg_parse_line("GARBAGE", &c));
    char buf[64];
    int n = rpmsg_format_telemetry(buf, sizeof(buf), 1234, 100.5f, 100.0f, 0.42f, 2);
    assert(n > 0 && strncmp(buf, "T 1234 100.5 100.0 0.420 2", 26) == 0);
    printf("PROTOCOL OK\n");
    return 0;
}
```

- [ ] **Step 5: 写一键测试脚本**

`tests/run_tests.sh`:
```sh
#!/bin/sh
set -e
INC="m4-firmware/Core/Inc"; SRC="m4-firmware/Core/Src"
cc -I$INC tests/test_pid.c      $SRC/pid.c        -lm -o /tmp/t_pid      && /tmp/t_pid
cc -I$INC tests/test_encoder.c  $SRC/encoder.c    -lm -o /tmp/t_enc      && /tmp/t_enc
cc -I$INC tests/test_protocol.c $SRC/rpmsg_comm.c     -o /tmp/t_proto    && /tmp/t_proto
echo "ALL UNIT TESTS PASSED"
```

- [ ] **Step 6: 运行全部并提交**

Run: `sh tests/run_tests.sh`
Expected:`PID OK` / `ENCODER OK` / `PROTOCOL OK` / `ALL UNIT TESTS PASSED`
```bash
git add tests/
git commit -m "test: host unit tests for pid, encoder, rpmsg protocol"
```

---

## Task 2(M1):M4 开环 PWM + A7 桥接 + Web 调占空比

**Files:**
- 集成:`m4-firmware/Core/Src/{motor_pwm,rpmsg_comm,app_control}.c` 进 CubeMX 工程
- 使用:`a7-bridge/bridge.py`、`web/`

**Interfaces:**
- Consumes:Task 0 的 RPMsg 通道。
- Produces:浏览器滑块 → 电机开环转速变化(尚无闭环)。

- [ ] **Step 1: M4 侧接好 PWM/方向,先做开环**

在 CubeMX 工程里实现 `motor_pwm.c` 的 TODO(PWM 定时器句柄、AIN1/AIN2);临时在 RX 回调里把 `SPD` 值当作占空比百分比直接 `motor_pwm_set(spd/3000.0f)`(开环,验证驱动)。编译烧写。

- [ ] **Step 2: 启动 A7 桥接**

板上:
```sh
cd a7-bridge && pip install -r requirements.txt
python3 bridge.py --tty /dev/ttyRPMSG0 --port 8080
```
Expected:打印 `bridge on :8080 tty=/dev/ttyRPMSG0`。

- [ ] **Step 3: 浏览器验证开环调速**

PC 浏览器打开 `http://<板子IP>:8080`,点"启动",拖"目标转速"滑块。
Expected:电机转速随滑块变化(此时是开环,转速不精确但能动)。

- [ ] **Step 4: 记录证据 + 提交**

保存演示照片/录屏说明到 `docs/evidence/m1-openloop.md`。
```bash
git add docs/evidence/m1-openloop.md
git commit -m "M1: open-loop PWM driven by web slider end-to-end"
```

---

## Task 3(M2):编码器测速 + PID 闭环 + 在线整定

**Files:**
- 集成:`app_control.c`(1 kHz 中断)、`encoder.c`;`board_read_encoder_count()` 实现

**Interfaces:**
- Consumes:Task 1 已验证的 `pid_update`/`encoder_update`;Task 2 的通道与 Web。
- Produces:闭环转速跟随 + Web 曲线 + 在线调 PID。

- [ ] **Step 1: 接上 1 kHz 控制中断**

CubeMX 配一个定时器 1 kHz 中断,回调里调用 `app_control_tick()`;主循环调用 `app_control_poll()`;实现:
```c
int32_t board_read_encoder_count(void) {
    return (int32_t)__HAL_TIM_GET_COUNTER(&htimENC);  /* 换成你的编码器定时器 */
}
```
把 RX 回调改为解析并应用命令:
```c
/* 在 OpenAMP RX 回调里 */
motor_cmd_t c;
if (rpmsg_parse_line((char*)rx_buf, &c)) app_control_apply(&c);
```
在 `app_control.c` 顶部按你的电机设置 `ENCODER_CPR`。

- [ ] **Step 2: 校准 ENCODER_CPR(转速标定)**

手动转一圈,读 `board_read_encoder_count()` 变化;或先给固定占空比,用示波器/转速表比对 Web 显示的 rpm,调 `ENCODER_CPR` 使显示与实际一致。
Expected:静止 rpm≈0;固定占空比时 Web rpm 与实测吻合(误差 < 5%)。

- [ ] **Step 3: 闭环跟随验证**

Web 点"启动",设目标 1000 rpm。
Expected:实测曲线在 1~2 秒内收敛到 1000 附近并稳住(可能有稳态误差,下一步调 PID)。

- [ ] **Step 4: 在线整定 PID**

拖 Kp/Ki/Kd 滑块点"应用 PID",观察阶跃响应变化。
Expected:增大 Ki 稳态误差减小;Kp 过大出现振荡——**曲线肉眼可见变化即证明在线整定链路通**。记下一组好参数写回 `app_control.c` 默认值。

- [ ] **Step 5: 记录证据 + 提交**

保存目标 vs 实测阶跃曲线截图到 `docs/evidence/m2-closedloop.png`(或说明)。
```bash
git add docs/evidence/ m4-firmware/Core/Src/app_control.c
git commit -m "M2: encoder speed + PID closed loop with online tuning"
```

---

## Task 4(M3):健壮性(堵转 / 失联 / 重连 / 启动顺序)

**Files:**
- Modify: `m4-firmware/Core/Src/app_control.c`(失联安全态)
- Modify: `a7-bridge/bridge.py`(已含重连;补 M4 崩溃提示)
- Create: `scripts/start_all.sh`(启动顺序)

**Interfaces:**
- Consumes:前序全部。
- Produces:异常场景下系统安全、可恢复。

- [ ] **Step 1: 验证堵转保护**

电机运行中用手轻轻堵住(注意安全、低速)。
Expected:约 300 ms 后 Web 弹出"堵转保护"且电机停;flags bit0 置位。(逻辑已在 `app_control.c`。)

- [ ] **Step 2: 加 RPMsg 失联安全态**

在 `app_control.c` 加"命令看门狗":若超过 1 秒没收到任何下行命令则停机。加计数器 `s_cmd_age`(每 tick +1,`app_control_apply` 里清零),在 `app_control_tick` 里:
```c
if (s_running && ++s_cmd_age > 1000) { s_running = 0; motor_pwm_stop(); s_flags &= ~FLAG_RUNNING; }
```
Expected:拔掉/关闭 Web 后 1 秒电机自动停(安全)。

- [ ] **Step 3: 写启动顺序脚本**

`scripts/start_all.sh`:
```sh
#!/bin/sh
set -e
sh "$(dirname "$0")/load_m4.sh" start motor_ctrl.elf
sleep 1
[ -e /dev/ttyRPMSG0 ] || { echo "no ttyRPMSG0"; exit 1; }
cd "$(dirname "$0")/../a7-bridge" && python3 bridge.py --tty /dev/ttyRPMSG0 --port 8080
```
Expected:一条命令完成"加载 M4 → 起桥接";顺序错误有明确报错。

- [ ] **Step 4: 验证断线重连**

运行中断开 WiFi/刷新页面再连。
Expected:Web 自动重连(app.js 已实现),曲线继续;M4 若被 `remoteproc stop` 再 start,桥接自动重开通道。

- [ ] **Step 5: 提交**

```bash
git add m4-firmware/Core/Src/app_control.c scripts/start_all.sh docs/evidence/
git commit -m "M3: robustness - stall/comm-loss safe state, startup ordering, reconnect"
```

---

## Task 5(M4,可选):集成 MPU-6050 振动/倾角监测

**Files:**
- Modify: `a7-bridge/bridge.py`(增加 IIO 读取线程)
- Modify: `web/index.html` / `web/app.js`(增加振动/倾角显示)

**Interfaces:**
- Consumes:`xiangmu` 仓库的 MPU-6050 IIO 驱动(sysfs `in_accel_*_raw` / `in_accel_scale`)。
- Produces:同一面板显示电机平台振动幅度 / 倾角。

- [ ] **Step 1: A7 读 IIO 加速度**

在 `bridge.py` 加线程,周期读 `/sys/bus/iio/devices/iio:device0/in_accel_{x,y,z}_raw` 与 `in_accel_scale`,算合加速度(去重力后的振动幅度),经同一 WebSocket 以 `V <mag>` 帧广播:
```python
def _iio_reader():
    base="/sys/bus/iio/devices/iio:device0"
    def rd(a):
        return float(open("%s/%s"%(base,a)).read())
    while True:
        try:
            s=rd("in_accel_scale")
            import math
            ax,ay,az=(rd("in_accel_%s_raw"%c)*s for c in "xyz")
            mag=abs(math.sqrt(ax*ax+ay*ay+az*az)-9.81)
            _broadcast("V %.3f"%mag)
        except Exception: pass
        time.sleep(0.1)
```
在 `main()` 里 `threading.Thread(target=_iio_reader,daemon=True).start()`。

- [ ] **Step 2: Web 显示振动**

在 `app.js` 的 `onTelemetry` 前加分支:
```js
if (s[0] === 'V') { document.getElementById('vib').textContent = parseFloat(s.split(/\s+/)[1]).toFixed(2); return; }
```
在 `index.html` 的 readout 里加 `振动 <b id="vib">0</b> m/s²`。
Expected:敲击/加速电机平台时振动数值跳动。

- [ ] **Step 3: 提交**

```bash
git add a7-bridge/bridge.py web/
git commit -m "M4(optional): integrate MPU-6050 IIO vibration into dashboard"
```

---

## Self-Review 结论

- **Spec 覆盖:** 异构通道(T0)、纯逻辑正确性(T1)、开环端到端(T2)、编码器+PID闭环+在线整定(T3)、堵转/失联/重连/启动顺序(T4)、IIO 集成(T5)、协议/1kHz/50Hz/共地/限幅(Global Constraints)—— 均有对应。
- **无占位符:** 每步含实际代码/命令/预期现象。硬件相关待填项(HAL 句柄、ENCODER_CPR)在对应步骤明确标注为"按你工程/电机填入",非计划缺陷。
- **类型一致:** 使用的函数名与仓库已提交代码一致(`pid_update`/`encoder_update`/`rpmsg_parse_line`/`rpmsg_format_telemetry`/`app_control_apply` 等);新增 `s_cmd_age`、`board_read_encoder_count` 在引入处定义。
- **可先做的:** Task 1(主机单元测试)与 Task 0(环境自检)**现在就能开始**,不必等买电机。
