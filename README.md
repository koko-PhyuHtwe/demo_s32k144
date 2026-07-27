# S32K144 最小系统模板

S32 Design Studio (S32DS.3.4) 官方生成的最小系统模板，已配置外部晶振 + LED 闪烁 + OSIF 延时，可直接作为新工程的起点。

---

## 芯片规格

| 项目 | 内容 |
|---|---|
| MCU | **S32K144** (Cortex-M4F, LQFP100) |
| SDK | S32SDK_S32K1XX_RTM_4.0.2 |
| 外部晶振 | 8 MHz（SOSC） |
| SPLL 倍频 | ×40（输出 320 MHz） |
| 核心频率 | 80 MHz（HSRUN 模式） |
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
                    │         SPLL 倍频器 (×40)                    │
                    │         输出: 320 MHz                        │
                    └─────────────────┬───────────────────────────┘
                                      │
                    ┌─────────────────▼───────────────────────────┐
                    │         SPLLDIV1 (÷2)                        │
                    │         SPLL_CLK_OUT: 160 MHz                │
                    └─────────────────┬───────────────────────────┘
                                      │
              ┌───────────────────────┼───────────────────────┐
              │                       │                       │
    ┌─────────▼─────────┐   ┌────────▼────────┐   ┌─────────▼─────────┐
    │    DIVCORE (÷2)    │   │   DIVBUS (÷2)   │   │  DIVSLOW (÷4)    │
    │  CORE_CLK: 80 MHz  │   │  BUS_CLK: 40 MHz│   │ FLASH_CLK: 20 MHz│
    └────────────────────┘   └─────────────────┘   └───────────────────┘
```

### 时钟域频率表

| 时钟域 | 频率 | 用途 |
|--------|------|------|
| **CORE_CLK** | 80 MHz | CPU 核心 |
| **BUS_CLK** | 40 MHz | 总线外设（GPIO、UART 等） |
| **FLASH_CLK** | 20 MHz | Flash 存储器 |
| **SOSCDIV2_CLK** | 8 MHz | LPUART1 时钟源 |
| **FIRCDIV1_CLK** | 48 MHz | CLKOUT、RTC 时钟源 |

### 时钟模式

| 模式 | 时钟源 | CORE_CLK | BUS_CLK |
|------|--------|----------|---------|
| **RUN** | SPLL | 80 MHz | 40 MHz |
| **HSRUN** | SPLL | 80 MHz | 40 MHz |
| **VLPR** | SIRC | 4 MHz | 8 MHz |

---

## 工程结构

```
demo_s32k144/
├── src/
│   └── main.c                      ← 主程序（LED 闪烁 + OSIF 延时）
├── board/                          ← 板级配置（S32 Config Tools 生成）
│   ├── clock_config.c / .h         ← 时钟树配置（外部晶振 + SPLL）
│   ├── pin_mux.c / .h              ← 引脚复用配置
│   ├── peripherals_osif_1.c / .h   ← OSIF 外设配置
│   └── sdk_project_config.h        ← SDK 统一头文件
├── SDK/
│   ├── platform/drivers/inc/       ← 驱动头文件
│   │   ├── clock.h
│   │   ├── interrupt_manager.h
│   │   └── pins_driver.h
│   └── rtos/osif/                  ← OSIF 驱动（裸机版）
│       ├── osif.h
│       └── osif_baremetal.c        ← 含 SysTick 1ms 中断实现
├── Debug_Configurations/           ← J-Link / PEMicro 调试配置
├── Doxygen/                        ← 文档生成配置
├── .project / .cproject            ← S32DS Eclipse 工程文件
├── demo_s32k144.mex                ← S32 Config Tools 配置文件
└── README.md                       ← 本文件
```

---

## 已配置好的基础设施

### 引脚分配

| 引脚 | 功能 | 说明 |
|------|------|------|
| **PTD15** | GPIO 输出 | LED0（红），高电平点亮 |
| **PTD16** | GPIO 输出 | LED1（绿），高电平点亮 |
| **PTA4** | SWD_DIO | SWD 调试数据线，不可占用 |
| **PTC4** | SWD_CLK | SWD 调试时钟线，不可占用 |

> ⚠️ **LPUART1 引脚（PTC6/PTC7）尚未配置**，如需使用 UART 请在 S32 Config Tools 中配置。

### SysTick（1 ms 节拍）

通过 OSIF 裸机模块自动管理：
- 首次调用 `OSIF_TimeDelay()` 时自动初始化 SysTick
- `OSIF_GetMilliseconds()` 获取系统运行毫秒数
- `SysTick_Handler` 在 `SDK/rtos/osif/osif_baremetal.c` 中实现

### LPUART1 时钟

已配置时钟源为 `SCG.SOSCDIV2_CLK`（8 MHz，来自外部晶振），但时钟门控当前为 **Disabled**。

> 如需启用 LPUART1，需在 `clock_config.c` 中将 `LPUART1_CGC` 改为 `Enabled`。

---

## main.c 功能说明

当前程序实现了 LED 闪烁功能：

```c
#include "sdk_project_config.h"
#include "osif.h"

#define LED0_PORT   PTD
#define LED0_PIN    15
#define LED1_PORT   PTD
#define LED1_PIN    16

int main(void)
{
    /* 1. 初始化时钟（外部晶振 + SPLL） */
    CLOCK_DRV_Init(&clockMan1_InitConfig0);

    /* 2. 初始化引脚 */
    PINS_DRV_Init(NUM_OF_CONFIGURED_PINS0, g_pin_mux_InitConfigArr0);

    /* 3. LED0 亮，LED1 灭 */
    PINS_DRV_SetPins(LED0_PORT, 1u << LED0_PIN);
    PINS_DRV_ClearPins(LED1_PORT, 1u << LED1_PIN);

    /* 4. 主循环：LED 交替闪烁 */
    for (;;)
    {
        OSIF_TimeDelay(1000);               // 延时 1 秒
        PINS_DRV_TogglePins(LED0_PORT, 1u << LED0_PIN);
        PINS_DRV_TogglePins(LED1_PORT, 1u << LED1_PIN);
    }
}
```

---

## 快速开始

### 1. 硬件准备

- S32K144EVB-Q100 开发板（或自定义 S32K144 板）
- 8 MHz 外部晶振（已配置，如使用内部时钟需修改 `clock_config.c`）
- J-Link / PEMicro 调试器
- USB 供电（或 12V 适配器）

### 2. 用 S32DS 导入工程

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

支持 **J-Link** 和 **PEMicro** 两种调试器。

---

## 扩展新外设

在 S32DS 中双击 `demo_s32k144.mex` → 打开 **S32 Configuration Tools**：

| 想添加 | 操作步骤 |
|--------|---------|
| **LPUART 串口** | Pins 工具选择 LPUART TX/RX 引脚 → Clocks 工具使能 LPUART 时钟门控 → 代码生成 |
| **FTM PWM** | Pins 工具选择 FTM 通道引脚 → Clocks 工具使能 FTM 时钟 |
| **FlexCAN** | Peripherals 工具添加 CAN 组件 → 配置波特率 |
| **GPIO 中断** | Pins 工具配置引脚为 GPIO + 中断使能 |
| **SPI 通信** | Pins 工具配置 LPSPI 引脚 → Clocks 工具使能 LPSPI 时钟 |

配置完成后点击 **Update Code**，工具会自动更新 `board/` 目录下的 `.c/.h` 文件。

---

## 注意事项

1. **外部晶振**：本工程使用 8 MHz 外部晶振。如开发板使用其他频率晶振，需修改 `clock_config.c` 中 `soscConfig.freq` 并重新计算 SPLL 倍频。
2. **内部时钟切换**：如无需外部晶振，可将 `soscConfig.initialize` 设为 `false`，并将系统时钟源改回 FIRC（48 MHz）。
3. **不要手动修改 `board/` 目录下带 "This file was generated by..." 注释的文件**，否则下次用 Config Tools 更新时会被覆盖。
4. **`.settings/language.settings.xml` 已加入 `.gitignore`**，换台电脑导入工程后 IDE 会自动重新生成。
5. 如需更改为 **FreeRTOS** 版本，将工程宏定义 `USING_OS_BAREMETAL` 改为 `USING_OS_FREERTOS`，并链接 FreeRTOS 版本的 `osif_freertos.c`。

---

## 许可证

本模板基于 NXP S32SDK 生成，SDK 源码版权归 NXP 所有，遵循 SDK 自带许可条款。
