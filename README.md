# S32K144 最小系统模板

S32 Design Studio (S32DS.3.4) 官方生成的最小系统模板，已配置外部晶振 + LED 闪烁 + OSIF 延时 + LPUART1 串口 + FlexCAN2 中断接收回复，代码已模块化封装，可直接作为新工程的起点。

---

## 芯片规格

| 项目 | 内容 |
|---|---|
| MCU | **S32K144** (Cortex-M4F, LQFP100) |
| SDK | S32SDK_S32K1XX_RTM_4.0.2 |
| 外部晶振 | 8 MHz（SOSC） |
| SPLL 倍频 | ×40（VCO 320 MHz，输出 160 MHz） |
| 核心频率 | 80 MHz（RUN/HSRUN 模式） |
| Flash | 512 KB |
| SRAM | 60 KB |

---

## 时钟架构

```
        ┌─────────────────────────────────────────────┐
        │              外部晶振 (SOSC)                  │
        │              8 MHz                           │
        └─────────────────┬───────────────────────────┘
                          │
        ┌─────────────────▼───────────────────────────┐
        │    SPLL: VCO = 8MHz × 40 = 320 MHz          │
        │    SPLL_CLK_OUT = VCO ÷ 2 = 160 MHz         │
        └─────────────────┬───────────────────────────┘
                          │
        ┌─────────────────▼───────────────────────────┐
        │    DIVCORE (÷2)                              │
        │    CORE_CLK = 160 ÷ 2 = 80 MHz               │
        └─────────────────┬───────────────────────────┘
                          │
              ┌───────────┴───────────┐
              │                       │
    ┌─────────▼─────────┐   ┌─────────▼─────────┐
    │   DIVBUS (÷2)     │   │  DIVSLOW (÷4)     │
    │ BUS_CLK = 40 MHz  │   │ FLASH_CLK = 20 MHz│
    └────────────────────┘   └───────────────────┘
```

> 注：DIVBUS 和 DIVSLOW 都从 CORE_CLK 分频，是级联关系而非并列。

### 时钟域频率表

| 时钟域 | 频率 | 用途 |
|--------|------|------|
| **CORE_CLK** | 80 MHz | CPU 核心 |
| **BUS_CLK** | 40 MHz | 总线外设（GPIO、UART 等） |
| **FLASH_CLK** | 20 MHz | Flash 存储器 |
| **SOSCDIV2_CLK** | 8 MHz | LPUART1 时钟源 |
| **FIRCDIV1_CLK** | 48 MHz | CLKOUT、RTC 时钟源 |

---

## 工程结构

```
demo_s32k144/
├── src/                               ← 用户应用代码（模块化）
│   ├── main.c                         ← 主程序（初始化 + 主循环）
│   ├── led.h / led.c                  ← LED 驱动模块
│   ├── uart.h / uart.c                ← LPUART1 串口驱动模块
│   └── can.h / can.c                  ← FlexCAN2 驱动模块（中断接收 + 非阻塞发送）
├── board/                             ← 板级配置（S32 Config Tools 生成）
│   ├── clock_config.c / .h            ← 时钟树配置（外部晶振 + SPLL）
│   ├── pin_mux.c / .h                 ← 引脚复用配置
│   ├── peripherals_osif_1.c / .h      ← OSIF 外设配置
│   ├── peripherals_lpuart_1.c / .h    ← LPUART1 配置
│   ├── peripherals_flexcan_config_1.c / .h ← FlexCAN2 配置
│   └── sdk_project_config.h           ← SDK 统一头文件
├── SDK/                               ← NXP SDK 驱动库
│   ├── platform/drivers/inc/          ← 驱动头文件
│   ├── platform/drivers/src/          ← 驱动源文件
│   └── rtos/osif/                     ← OSIF 驱动（裸机版）
├── Debug_Configurations/              ← J-Link / PEMicro 调试配置
├── .project / .cproject               ← S32DS Eclipse 工程文件
├── demo_s32k144.mex                   ← S32 Config Tools 配置文件
└── README.md                          ← 本文件
```

---

## 引脚分配

| 引脚 | 功能 | 说明 |
|------|------|------|
| **PTD15** | GPIO 输出 | LED0（红），高电平点亮 |
| **PTD16** | GPIO 输出 | LED1（绿），高电平点亮 |
| **PTC6** | LPUART1_RX | 串口接收 |
| **PTC7** | LPUART1_TX | 串口发送 |
| **PTC16** | CAN2_RX | FlexCAN2 接收 |
| **PTC17** | CAN2_TX | FlexCAN2 发送 |
| **PTA4** | SWD_DIO | SWD 调试数据线，不可占用 |
| **PTC4** | SWD_CLK | SWD 调试时钟线，不可占用 |

---

## 模块说明

### LED 模块 (`led.h / led.c`)

| 函数 | 功能 |
|------|------|
| `LED_Init()` | 初始化 LED，LED0 亮、LED1 灭 |
| `LED_TurnOn(pin)` | 点亮指定 LED |
| `LED_TurnOff(pin)` | 熄灭指定 LED |
| `LED_Toggle(pin)` | 翻转指定 LED |
| `LED_ToggleBoth()` | 同时翻转两个 LED（心跳指示） |

### UART 模块 (`uart.h / uart.c`)

| 参数 | 值 |
|------|-----|
| 实例 | LPUART1 |
| 波特率 | 115200 |
| 数据格式 | 8N1 |
| 发送方式 | 轮询（`LPUART_DRV_SendDataPolling`） |

| 函数 | 功能 |
|------|------|
| `UART_Init()` | 初始化 LPUART1 |
| `UART_SendData(data, len)` | 轮询发送数据 |
| `UART_SendBootMessage()` | 发送系统启动信息 |

上电后串口输出：
```
========================================
  S32K144 System Boot
  CORE_CLK  = 80 MHz
  BUS_CLK   = 40 MHz
  FLASH_CLK = 20 MHz
  LPUART1   = 115200 8N1
========================================
```

### CAN 模块 (`can.h / can.c`)

| 参数 | 值 |
|------|-----|
| 实例 | FlexCAN2 |
| PE 时钟源 | OSC = 8 MHz |
| 工作模式 | Normal |
| 波特率 | 500 kbps |
| 采样点 | 87.5% |
| 帧格式 | 标准帧（11 位 ID） |

**邮箱分配：**

| 邮箱 | 方向 | ID | 说明 |
|------|------|----|------|
| M0 | 发送 | 0x123 | 回复数据 |
| M1 | 接收 | 0x7E0 | 接收 CAN 卡数据（ID 过滤） |

**波特率计算：**
```
TQ 总数 = 1(SYNC) + (PROPSEG+1) + (PSEG1+1) + (PSEG2+1)
         = 1 + 6 + 7 + 2 = 16
Bit Rate = 8MHz / (0+1) / 16 = 500,000 bps
采样点   = (1+6+7) / 16 = 87.5%
```

**工作流程（中断驱动）：**
```
CAN 卡发送 ID=0x7E0 → M1 接收（硬件 ID 过滤）
                              ↓
                    触发 MB 中断 → 回调函数
                              ↓
              回调中：1) 非阻塞发送回复 ID=0x123
                      2) 重新启动 M1 接收
                              ↓
                   等待下一帧接收...（循环）
```

**回复数据：**
```
ID: 0x123  DLC: 8  Data: 11 22 33 44 55 66 77 88
```

| 函数 | 功能 |
|------|------|
| `CAN_Init()` | 初始化 FlexCAN2，配置收发邮箱，启动中断接收 |
| `CAN_SendMessage(id, data, len)` | 轮询发送 CAN 消息 |
| `CAN_SendReply()` | 发送预设回复数据 |

---

## main.c 功能说明

```c
int main(void)
{
    /* 系统初始化 */
    CLOCK_DRV_Init(...);       // 时钟
    PINS_DRV_Init(...);        // 引脚

    /* 外设初始化 */
    LED_Init();                // LED
    UART_Init();               // 串口
    UART_SendBootMessage();    // 发送启动信息
    CAN_Init();                // CAN（配置收发邮箱 + 启动中断接收）

    /* 主循环 */
    for (;;)
    {
        OSIF_TimeDelay(1000);  // 延时 1 秒
        LED_ToggleBoth();      // LED 心跳闪烁
        // CAN 通信由中断自动处理，无需主循环干预
    }
}
```

---

## 快速开始

### 1. 硬件准备

- S32K144EVB-Q100 开发板（或自定义 S32K144 板）
- 8 MHz 外部晶振
- J-Link / PEMicro 调试器
- USB 供电
- CAN 分析仪（如 PCAN-USB），波特率设为 500 kbps

### 2. 导入工程

```
File → Import → Existing Projects into Workspace
→ 选择本工程根目录 → Finish
```

### 3. 编译

```
Project → Build Project
```
或按 `Ctrl + B`

### 4. 调试

| 配置 | 说明 |
|------|------|
| `Debug_FLASH` | Flash 调试（推荐，断电保存） |
| `Debug_RAM` | RAM 调试（快速，不擦 Flash） |

### 5. CAN 通信测试

1. CAN 分析仪设置波特率 500 kbps
2. CAN 分析仪发送：ID = 0x7E0，Data = 任意 8 字节
3. MCU 收到后自动回复：ID = 0x123，Data = 11 22 33 44 55 66 77 88

---

## 注意事项

1. **外部晶振**：本工程使用 8 MHz 外部晶振。如使用其他频率，需修改 `clock_config.c` 中 `soscConfig.freq` 并重新计算 SPLL 倍频。
2. **CAN 时钟**：FlexCAN2 的 PE 时钟由 `peripherals_flexcan_config_1.c` 中 `pe_clock = FLEXCAN_CLK_SOURCE_OSC` 指定（8 MHz），与 PCC 层的 `clkSrc` 无关。
3. **不要手动修改 `board/` 目录下带 "This file was generated by..." 注释的文件**，否则下次用 Config Tools 更新时会被覆盖。
4. **OSIF 模式**：当前为裸机模式。如需切换为 FreeRTOS，在预处理符号中定义 `USING_OS_FREERTOS`。
5. **中断接收注意**：`FLEXCAN_DRV_Receive()` 必须在 `ConfigRxMb()` 之后调用，否则 MB 状态不是 `RX_BUSY`，中断不会触发回调。回调中需再次调用以恢复接收能力。

---

## 许可证

本模板基于 NXP S32SDK 生成，SDK 源码版权归 NXP 所有，遵循 SDK 自带许可条款。
