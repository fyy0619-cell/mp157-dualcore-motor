# Step A/B 无电机全栈上板实录(真机验证)

> 在真板子(正点原子 ATK STM32MP157,内核 5.4.31 BSP)上,**不需要真电机**就把整条产品栈跑通:
> 浏览器 → A7 bridge → RPMsg → **M4(你写的 1kHz PID + 模拟电机)** → 回遥测 → 浏览器实时曲线。
> 这验证了"异构双核 + 实时控制 + Web 可视化"的全链路;电机到货后只需把"模拟电机"换成真 encoder+PWM。

## 架构

```
浏览器(Chart.js 控制台)
   │  ① 命令 RUN/SPD/PID (HTTP GET /cmd)      ② 遥测 T... (SSE /events)
   ▼                                           ▲
A7 bridge(a7-bridge/bridge_sim.py,开发期跑在 VM,隔 ssh 够板子)
   │  ssh printf > /dev/ttyRPMSG0              ssh tail -f /tmp/tele.txt
   ▼                                           ▲
板子:/dev/ttyRPMSG0  ←RPMsg→  M4 固件(1kHz PID + 一阶模型模拟电机)
             └─ 本地 setsid cat /dev/ttyRPMSG0 >> /tmp/tele.txt(瞬间接住,喂文件)
```

- **协议**(整数化,避开嵌入式浮点 printf/scanf):
  - 下行:`RUN 1|0` · `SPD <整数rpm>` · `PID <kp×1e6> <ki×1e6> <kd×1e6>`
  - 上行:`T <ms> <rpm> <target> <dutyPct> <flags>`(dutyPct=duty×100,flags bit1=运行中)
- **模拟电机**:一阶惯性 `rpm += (SIM_MAXRPM*duty - rpm) * (CTRL_DT/SIM_TAU)`,SIM_MAXRPM=3000,SIM_TAU=0.15,CTRL_DT=1ms。
- 实测:`SPD 1500`→rpm 追到 1500;`SPD 500`→落到 500;`RUN 0`→衰减到 0;调 Kp/Ki/Kd 阶跃响应实时变化。

## M4 固件做法:在跑通的 RPMsg_UART 例程上扩展(只改 USER CODE 区)

不从零建 CubeMX 工程,直接在正点原子 `RPMsg_UART` 例程的 `CM4/Core/Src/main.c` 的 USER CODE 段加:

```c
/* PV:PID + 模拟电机状态 */
#define CTRL_DT 0.001f
#define SIM_MAXRPM 3000.0f
#define SIM_TAU 0.15f
#define TELE_DECIM 20
static float g_kp=0.0008f,g_ki=0.0040f,g_kd=0.00002f, g_integ=0,g_prev=0, g_target=0,g_rpm=0,g_duty=0;
static int g_running=0;
static uint32_t g_ms=0,g_decim=0,g_lastms=0;

/* 位置式 PID(抗积分饱和+微分先行),输出钳 [-1,1] */
static float pid_update(float sp,float meas){ float e=sp-meas;
  float o=g_kp*e - g_kd*((meas-g_prev)/CTRL_DT); g_prev=meas;
  float in=g_integ+g_ki*e*CTRL_DT, out=o+in;
  if(out>1.f){if(e<0)g_integ=in; out=1.f;} else if(out<-1.f){if(e>0)g_integ=in; out=-1.f;} else g_integ=in;
  return out; }

/* 一步控制:PID→duty→一阶模型模拟转速 */
static void control_step(void){ g_ms++;
  if(g_running) g_duty=pid_update(g_target,g_rpm); else {g_duty=0;g_integ=0;g_prev=0;}
  g_rpm += (SIM_MAXRPM*g_duty - g_rpm)*(CTRL_DT/SIM_TAU); }

/* while(1) 内:OPENAMP_check_for_message() 之后 */
if (VirtUart0RxMsg){ VirtUart0RxMsg=RESET; char*c=(char*)VirtUart0ChannelBuffRx;
  if(!strncmp(c,"RUN",3)){int r=0;sscanf(c+3,"%d",&r); if(r&&!g_running){g_integ=0;g_prev=0;} g_running=r?1:0;}
  else if(!strncmp(c,"SPD",3)){int v=0;sscanf(c+3,"%d",&v); g_target=(float)v;}
  else if(!strncmp(c,"PID",3)){int a=0,b=0,d=0; if(sscanf(c+3,"%d %d %d",&a,&b,&d)==3){g_kp=a*1e-6f;g_ki=b*1e-6f;g_kd=d*1e-6f;}}
  memset(VirtUart0ChannelBuffRx,0,VirtUart0ChannelRxSize); }
{ uint32_t now=HAL_GetTick();             /* 单步!每轮最多走一步、发一条 */
  if(now!=g_lastms){ g_lastms=now; control_step();
    if(++g_decim>=TELE_DECIM){ g_decim=0; int dp=(int)(g_duty*100.f);
      int n=snprintf((char*)BuffTx,MAX_BUFFER_SIZE,"T %lu %d %d %d %d\n",
        (unsigned long)g_ms,(int)g_rpm,(int)g_target,dp,g_running?2:0);
      if(n>0) VIRT_UART_Transmit(&huart0,BuffTx,(uint16_t)n); } } }
```

## ⚠️ 三个血泪坑(M1 换真电机也适用)

1. **M4 发送绝不能批量**。一个主循环迭代里连发多条 `VIRT_UART_Transmit` 而中间不回 `OPENAMP_check_for_message()`
   → TX vring(16 缓冲)灌满 → `VIRT_UART_Transmit` **阻塞死** → M4 卡住、串流和命令全停。
   **必须每轮最多发一条**(上面的单步写法:`if(now!=g_lastms){...一步...一条遥测...}`,不要 `while(catch-up)` 批量)。
2. **定时用 `HAL_GetTick()` 即可**(SysTick 在 remoteproc/协处理器模式下正常走);
   **不要访问 DWT 寄存器**(`CoreDebug->DEMCR`/`DWT->CYCCNT`)——会把 M4 搞挂,连 RX 都不响应。
3. **M4 要先"收到过一条命令"才会开始发遥测**(ST VIRT_UART 的发送目标 dst 靠首次 RX 建立),
   **且读者必须在 M4 一启动就挂上**(趁 16 缓冲没满),否则无读者期 M4 发满即阻塞。

## 稳健启动顺序(板子重启后要重做)

```sh
# 板子上:加载 M4 → 本地 cat 瞬间接住喂文件 → 发一条 priming 命令建立 dst
echo RPMsg_UART_CM4.elf > /sys/class/remoteproc/remoteproc0/firmware
echo start > /sys/class/remoteproc/remoteproc0/state
while [ ! -e /dev/ttyRPMSG0 ]; do : ; done
: > /tmp/tele.txt
setsid sh -c "cat /dev/ttyRPMSG0 >> /tmp/tele.txt" </dev/null >/dev/null 2>&1 &
sleep 0.5 ; printf "SPD 0\n" > /dev/ttyRPMSG0
```

```sh
# VM 上:bridge 独立常驻(别用 held-ssh 后台,会掉线)
setsid sh -c "cd ~ && python3 -u bridge_sim.py" >/tmp/bridge.log 2>&1 </dev/null &
# 浏览器:http://<VM-ip>:8080
```

> bridge 读的是 `ssh tail -n1 -f /tmp/tele.txt`(读文件、不抢 tty),写命令是 `ssh printf > /dev/ttyRPMSG0`,两者并存。
> **产品化**:把 bridge 从 VM 搬到板子 A7(交叉编译的 C 小程序,或给 rootfs 加 Python),去掉 ssh 中转这一跳。
