# M1 真电机上板(TB6612 + JGA25-370 编码器电机)

> 把 Step B 的"模拟电机"换成真硬件:**编码器测速 + PWM/方向驱动**。上层(PID / RPMsg 协议 / bridge / web)完全不变。
> 硬件:TB6612FNG 驱动 + JGA25-370(DC6V 620rpm,11 线霍尔编码器);电机电源 5V(额定 6V,5V 约 500rpm,无害)。

## 引脚分配(正点原子 ATK STM32MP157,JP1 排针,全在空闲的 SAI2 区)

音频(SAI2)和定时器 Linux 都没用 → 这一排全空;编码器用 **GPIO 轮询**(省掉 EXTI/NVIC)。

| 信号 | M4 引脚 | JP1脚 / 丝印 | 说明 |
|---|---|---|---|
| PWMA | PE14 | 17 / SAI2_MCLKB | TIM1_CH4,PWM 调速 |
| AIN1 | PE13 | 18 / SAI2_FSB | 方向 |
| AIN2 | PE12 | 20 / SAI2_SCKB | 方向 |
| STBY | PE0  | 22 / SAI2_MCLKA | 使能(拉高) |
| 编码器A(C1) | PI5 | 21 / SAI2_SCKA | 轮询计数 |
| 编码器B(C2) | PI6 | 19 / SAI2_SDA | 判方向 |

- TB6612:VM←外部 5V,VCC←板 3.3V,GND←板 GND(**三方共地**),AO1/AO2→电机 1/6 脚。
- 编码器:+5/3.3V ← 板 3.3V,GND ← 板 GND,C1/C2 → PI5/PI6。

## 工程配置(只需一个宏)

RPMsg_UART 例程本不含定时器。`CM4/Core/Inc/stm32mp1xx_hal_conf.h` 里**打开** `#define HAL_TIM_MODULE_ENABLED`
(`hal_tim.c`/`hal_tim_ex.c` 已在 `.project` 的 linkedResources 里,只是被宏关着编成空)。**无需 CubeMX 重新生成**。

## 关键代码(全在 main.c 的 USER CODE 区手写)

```c
TIM_HandleTypeDef htim1;            /* PWM: TIM1_CH4 -> PE14 */
#define ENC_CPR 231.0f             /* 每输出轴一圈计数 = 11 x 减速比;转速不准就标定这里 */

/* 把 duty(-1..1) 施加到电机:方向(AIN1/AIN2) + PWM 幅度 */
static void apply_motor(float duty){
  int fwd = (duty >= 0.0f); float m = fwd?duty:-duty; if(m>1.f)m=1.f;
  HAL_GPIO_WritePin(GPIOE,GPIO_PIN_13, fwd?GPIO_PIN_SET:GPIO_PIN_RESET);   /* AIN1 */
  HAL_GPIO_WritePin(GPIOE,GPIO_PIN_12, fwd?GPIO_PIN_RESET:GPIO_PIN_SET);   /* AIN2 */
  __HAL_TIM_SET_COMPARE(&htim1,TIM_CHANNEL_4,(uint32_t)(m*999.0f));
}
/* 编码器轮询(主循环高速调用):A 相上升沿 + B 相判方向 */
static void encoder_poll(void){
  int a = (HAL_GPIO_ReadPin(GPIOI,GPIO_PIN_5)==GPIO_PIN_SET);
  if(a && !g_a_prev){ int b=(HAL_GPIO_ReadPin(GPIOI,GPIO_PIN_6)==GPIO_PIN_SET); g_enc += b?-1:+1; }
  g_a_prev = a;
}
/* 1ms 控制:编码器增量算 rpm -> PID -> 施加 */
static void control_step(void){
  g_ms++; int32_t e=g_enc; float d=(float)(e-g_enc_prev); g_enc_prev=e;
  float rpm = d/ENC_CPR/0.001f*60.f; g_rpm = g_rpm*0.8f + rpm*0.2f;   /* 轻度低通 */
  if(g_running) g_duty=pid_update(g_target,g_rpm); else {g_duty=0;g_integ=0;g_prev=0;}
  apply_motor(g_duty);
}
/* motor_hw_init(): 开 GPIOE/GPIOI/TIM1 时钟,PE13/12/0 输出(STBY 拉高),PI5/6 输入上拉,
   PE14=AF1_TIM1,TIM1 PWM1 CH4 @ PSC=9/ARR=999(~20kHz),HAL_TIM_PWM_Start + __HAL_TIM_MOE_ENABLE */
```

## 上板验证要点 & 常见坑

- **编码器先验**:不驱动电机,手转输出轴 → 看 rpm 变化、正负号(验 PI5/PI6 + 供电)。
- **方向符号必须一致**:若"电机转向"与"编码器计数符号"相反 → 正反馈 → **电机瞬间飙满速**。
  修:`apply_motor` 里对调 AIN1/AIN2,或 `encoder_poll` 里 `g_enc` 增量取反。第一次驱动务必小目标 + 手指放停止。
- **转速标定**:`ENC_CPR` 先按 11×减速比估,显示不准就按"手转已知圈数数计数"标定。
- **5V 电机**:最高约 500rpm,web 目标滑块上限设 500 更合适。

## 启动顺序(同 Step B,板子重启后重做)

```sh
echo RPMsg_UART_CM4.elf > /sys/class/remoteproc/remoteproc0/firmware
echo start > /sys/class/remoteproc/remoteproc0/state
while [ ! -e /dev/ttyRPMSG0 ]; do : ; done
: > /tmp/tele.txt
setsid sh -c "cat /dev/ttyRPMSG0 >> /tmp/tele.txt" </dev/null >/dev/null 2>&1 &
sleep 0.5 ; printf "RUN 0\n" > /dev/ttyRPMSG0     # 挂读者在前 + 首次 RX 建 dst(见 STEP-AB 三坑)
```
