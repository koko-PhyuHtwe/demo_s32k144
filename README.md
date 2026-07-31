# S32K144 Bootloader + App 完整工程

基于 S32 Design Studio (S32DS.3.4) + S32K144 (Cortex-M4F) 的 **CAN Bootloader + App** 一体化工程。
使用单个工程、两个构建配置，分别生成 Bootloader 和 App 固件。

当前版本：**v1.0 完整 Bootloader 链路**（全部功能验证通过 ✅）。

---

## 开发阶段记录

| 阶段 | 内容 | 状态 | 提交标记 |
|------|------|------|----------|
| v0.1 | 最小系统模板（时钟 + LED + UART + CAN 基础收发） | ✅ 完成 | init-template |
| v0.2 | CAN 环形队列改造，解耦中断与业务 | ✅ 完成 | can-ring-buffer |
| v0.3 | UDS 基础服务（0x10 / 0x11 / 0x22） | ✅ 完成 | uds-basic |
| v0.4 | Flash 驱动集成 + UDS 0x34/0x36/0x37 下载链路 | ✅ 完成 | flash-download |
| v0.5 | S3Server 超时保护 + 自动跳 App | ✅ 完成 | s3-timeout-jump |
| v0.6 | Boot 模式选择（上电先尝试跳 App） | ✅ 完成 | boot-mode-select |
| v0.7 | 固件 CRC32 校验（0x2E WriteDID） | ✅ 完成 | crc32-verify |
| v0.8 | 双构建配置 + Boot↔App 双向跳转 | ✅ 完成 | dual-build-config |
| v0.9 | UART 中断接收 + App 触发升级（发 'U'） | ✅ 完成 | uart-upgrade-trigger |
| **v1.0** | **CANoe 自动刷写脚本 + 完整链路验证** | ✅ 完成 | **final-canoe-script** |

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

## Flash 分区规划

```
0x00000000 ┌────────────────────────────┐
           │                            │
           │   Bootloader 区域          │  64 KB
           │   (Debug_FLASH 构建)       │
           │                            │
0x0000FFF0 ├────────────────────────────┤  ← 升级标志位 UPGRADE_FLAG_ADDR
           │                            │
0x00010000 ├────────────────────────────┤  ← App 起始地址 APP_FLASH_BASE
           │                            │
           │   App 区域                  │  约 448 KB
           │   (Debug_APP 构建)         │
           │                            │
0x00080000 └────────────────────────────┘
```

**升级标志位定义（[app.h](file:///c:/Users/10608/Documents/NXP_S32_3.4/demo_s32k144/src/app.h)）：**
```c
#define UPGRADE_FLAG_ADDR     0x0003FFF0U
#define UPGRADE_FLAG_MAGIC    0x5AA55AA5U
```

- App 想升级时 → 把 `0x5AA55AA5` 写入 `0x0003FFF0` → 系统复位 → Bootloader 检测到 magic number → 停在 Bootloader 等 UDS
- UDS CRC 校验通过 → Bootloader 擦除 `0x0003FFF0` 所在扇区 → CANoe 发 0x11 ECU Reset → 正常跳 App

---

## 双构建配置

单个 S32DS 工程中创建了两个构建配置：

| 构建配置 | 链接脚本 | 预处理宏 | 起始地址 | 用途 |
|---------|---------|---------|---------|------|
| **Debug_FLASH** | `S32K144_64_flash.ld` | 无 | 0x00000000 | Bootloader 固件 |
| **Debug_APP** | `S32K144_64_app.ld` | `BUILD_APP` | 0x00010000 | App 固件 |

同一套源文件，[main.c](file:///c:/Users/10608/Documents/NXP_S32_3.4/demo_s32k144/src/main.c) 中用 `#ifdef BUILD_APP` 切换：
```c
#ifdef BUILD_APP
/* App 构建：只调用 app_main() */
int main(void) { return app_main(); }
#else
/* Bootloader 构建：完整初始化 + UDS 循环 */
int main(void) { ... }
#endif
```

---

## 工程结构

```
demo_s32k144/
├── src/                               ← 公共源代码
│   ├── main.c                         ← 主程序（双构建入口）
│   ├── app.h / app.c                  ← App 主逻辑 + 升级标志定义
│   ├── led.h / led.c                  ← LED 驱动
│   ├── uart.h / uart.c                ← LPUART1 驱动（中断接收）
│   ├── can.h / can.c                  ← FlexCAN2 驱动（环形队列）
│   ├── uds.h / uds.c                  ← UDS 服务 + S3Server 超时 + 跳转
│   └── flash_app.h / flash_app.c      ← Flash 封装（擦写校验/CRC32）
├── board/                             ← 板级配置（Config Tools 生成）
│   ├── clock_config.c / .h
│   ├── pin_mux.c / .h
│   ├── peripherals_osif_1.c / .h
│   ├── peripherals_lpuart_1.c / .h
│   ├── peripherals_flexcan_config_1.c / .h
│   ├── peripherals_flash_1.c / .h
│   └── sdk_project_config.h
├── Project_Settings/Linker_Files/
│   ├── S32K144_64_flash.ld            ← Bootloader 链接脚本
│   └── S32K144_64_app.ld              ← App 链接脚本
├── .project / .cproject               ← S32DS Eclipse 工程文件
│
├── uds_download.can                   ← CANoe CAPL 自动升级脚本
├── uds_download.cbf                   ← CANoe 数据库（CAPL 配置）
├── uds_frames.csv                     ← CANoe 帧配置
├── uds_download.py                    ← Python 离线 UDS 报文生成工具
├── check_crc32.py                      ← CRC32 校验工具
├── app.bin                            ← App 固件（CANoe 刷写用）
│
├── .gitignore
└── README.md
```

---

## 引脚分配

| 引脚 | 功能 | 说明 |
|------|------|------|
| **PTD15** | GPIO 输出 | LED0（绿灯），高电平点亮 |
| **PTD16** | GPIO 输出 | LED1（蓝灯），高电平点亮 |
| **PTC6** | LPUART1_RX | 串口接收 |
| **PTC7** | LPUART1_TX | 串口发送 |
| **PTC16** | CAN2_RX | FlexCAN2 接收 |
| **PTC17** | CAN2_TX | FlexCAN2 发送 |
| **PTA4** | SWD_DIO | SWD 调试数据线 |
| **PTC4** | SWD_CLK | SWD 调试时钟线 |

---

## Boot 模式流程图

```
上电复位
    │
    ▼
┌──────────────────────────────────┐
│  Bootloader 启动                  │
│  - 初始化时钟/引脚/LED/UART       │
│  - 打印启动信息                   │
└──────────────┬───────────────────┘
               │
               ▼
┌──────────────────────────────────┐
│  检查 0x0003FFF0 处的标志？        │
│  upgradeFlag = *(0x0003FFF0)      │
└──────┬─────────────────┬──────────┘
       │ == MAGIC         │ != MAGIC
       ▼                  ▼
┌───────────────┐    ┌─────────────────────────┐
│ 停在 Bootloader│    │ 尝试跳转到 App？          │
│ (等 UDS 下载)   │    │ - 读 App MSP/Reset向量  │
└───────┬───────┘    │ - 检查地址合法性          │
        │            └──────┬──────────────┬────┘
        ▼                   │ 合法         │ 不合法
┌────────────────────┐      ▼              ▼
│ UDS 升级流程        │   跳转成功          ┌──────────────┐
│ 0x10→0x34→0x36×N   │   进入 App          │ 5 秒 S3Server │
│     →0x37→0x2E     │                     │   倒计时     │
│     →0x11 Reset    │                     └──────┬───────┘
└────────┬───────────┘                            │
         │ 擦除标志位                              │ 倒计时结束
         ▼                                        ▼
    完整复位 → 走 "无标志 → 跳 App" 路径        回到 "尝试跳 App"
```

---

## LED 状态指示

| 状态 | 蓝灯 (LED1/PTD16) | 绿灯 (LED0/PTD15) |
|------|------------------|------------------|
| **Bootloader 运行中** | 500ms 交替闪烁 | 500ms 交替闪烁 |
| **App 正常运行** | **常亮**（App 标志） | **200ms 快速闪烁**（App 心跳） |
| **卡死** | 常亮 | 不亮 |

---

## UDS 诊断服务完整列表

| SID | 服务 | 子功能/DID | 说明 |
|-----|------|-----------|------|
| **0x10** | 会话控制 | 0x01 默认 / 0x03 扩展 | 切换会话，重置 S3Server 计时器 |
| **0x11** | ECU 复位 | 0x01 硬复位 | 100ms 后 NVIC 系统复位 |
| **0x22** | 读 DID | 0xF189 | 读版本号 "V1.0" |
| **0x2E** | 写 DID | 0xFF01 (4 字节 CRC32) | 校验 App Flash 完整性，成功则清除升级标志 |
| **0x34** | 请求下载 | 4 字节地址 + 2 字节大小 | 擦除目标扇区，进入升级模式 |
| **0x36** | 传输数据 | BlockNumber + 4 字节数据 | 攒 8 字节写一次 Flash |
| **0x37** | 退出传输 | 无 | 补 0xFF 写入剩余数据，退出升级模式 |

### 完整 UDS 升级流程（6 步）

```
Step 1: 0x10 03            → 扩展会话 (正响应 0x50 03)
Step 2: 0x34 00 01 00 00 XX YY   → 请求下载 (addr=0x00010000, size=0xXXYY)
                           ← 正响应 0x74 20 04 04
Step 3: 0x36 <BlockNum> <4字节数据> × N   → 传输数据块 (正响应 0x76 <BlockNum>)
Step 4: 0x37 00            → 退出传输 (正响应 0x77)
Step 5: 0x2E FF 01 <CRC[3:0]>  → 写 CRC32 校验 (正响应 0x6E FF 01 00)
                           → Bootloader: CRC OK → 清除升级标志
Step 6: 0x11 01            → ECU 复位 (正响应 0x51 01)
                           → 芯片完整硬件复位
                           → Bootloader: 无标志 → 跳 App
```

---

## S3Server 超时机制

| 条件 | 行为 |
|------|------|
| 上电后 5 秒内无有效 UDS 请求 | 尝试跳转到 App |
| 收到任何有效 UDS 正响应/请求 | 重置计时器为 5 秒 |
| 升级中（0x34 之后、0x37 之前） | 超时不跳，继续等待（防止中断写入） |
| App 合法性校验 | MSP 必须在 SRAM 范围 (0x20000000~0x20010000)，PC 必须在 App Flash 范围 (0x00010000~0x0007FFFF)，否则留在 Bootloader |

---

## UART 命令接口（App 运行时）

| 命令 | 说明 |
|------|------|
| **U** 或 **u** | 触发升级：写升级标志 `0x5AA55AA5` 到 `0x0003FFF0` → 系统复位 → 进入 Bootloader |

---

## CANoe 自动刷写说明

提供了 [uds_download.can](file:///c:/Users/10608/Documents/NXP_S32_3.4/demo_s32k144/uds_download.can)（CAPL 脚本），自动完成上述 6 步 UDS 流程。

### 使用方法

1. 将生成的 **app.bin** 复制到 CANoe `.cfg` 配置文件同一目录
2. 打开 CANoe，配置 CAN 通道波特率 500kbps，ID 过滤关闭
3. 载入 `uds_download.can` 脚本
4. App 运行时串口发 'U' 进入 Bootloader（或上电无 App 时自动等待）
5. 点击 CANoe 开始测量 → 自动完成刷写 → 复位 → 跳转新 App

### CAN ID 约定

| 方向 | CAN ID | 说明 |
|------|--------|------|
| 上位机 → 板卡 | **0x7E0** | UDS 请求（CAPL 发送） |
| 板卡 → 上位机 | **0x123** | UDS 响应（Bootloader 回复） |

---

## 模块说明

### LED 模块 (`led.h / led.c`)

| 函数 | 功能 |
|------|------|
| `LED_Init()` | 初始化 LED，LED0 亮、LED1 灭 |
| `LED_TurnOn(pin)` | 点亮指定 LED |
| `LED_TurnOff(pin)` | 熄灭指定 LED |
| `LED_Toggle(pin)` | 翻转指定 LED |
| `LED_ToggleBoth()` | 同时翻转两个 LED |

### UART 模块 (`uart.h / uart.c`)

| 参数 | 值 |
|------|-----|
| 实例 | LPUART1 |
| 波特率 | 115200 |
| 格式 | 8N1 |
| 发送 | 轮询 `LPUART_DRV_SendDataPolling` |
| 接收 | SDK 异步中断 + 回调（`uart_rx_callback` → `g_rxReady` 标志） |

上电串口输出：
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

**邮箱分配：**

| 邮箱 | 方向 | ID | 说明 |
|------|------|----|------|
| M0 | 发送 | 0x123 | UDS 正/负响应 |
| M1 | 接收 | 0x7E0 | UDS 请求（ID 过滤） |

**波特率计算：**
```
TQ 总数 = 1(SYNC) + (PROPSEG+1) + (PSEG1+1) + (PSEG2+1)
         = 1 + 6 + 7 + 2 = 16
Bit Rate = 8MHz / (0+1) / 16 = 500,000 bps
采样点   = (1+6+7) / 16 = 87.5%
```

**环形队列：** 中断（ISR）写队列，主循环读队列处理。

### Flash 封装模块 (`flash_app.h / flash_app.c`)

基于 NXP C40ASF Flash 驱动。

| 操作 | 地址对齐 | 大小对齐 |
|------|----------|----------|
| 擦除 | 16 字节 | 4KB（扇区大小） |
| 写入 | 8 字节 | 8 字节 |

| 函数 | 功能 |
|------|------|
| `FlashApp_Init()` | 初始化 Flash 驱动 |
| `FlashApp_Erase(addr, size)` | 擦除指定区域，拒绝擦除 Bootloader 区 |
| `FlashApp_Write(addr, data, size)` | 写入数据（必须 8 字节对齐） |
| `FlashApp_Verify(addr, expected, size)` | 读回校验 Flash |
| `FlashApp_CalcCRC32(addr, size)` | 查表法 CRC-32/ISO-HDLC |

---

## 快速开始

### 1. 编译两个固件

1. S32DS 中选中 **Debug_FLASH** 构建配置 → `Ctrl+B` 编译
2. 切换构建配置为 **Debug_APP** → `Ctrl+B` 编译

### 2. 生成 CANoe 用的 app.bin

```powershell
cd c:\Users\10608\Documents\NXP_S32_3.4\demo_s32k144
& "E:\NXP\S32DS.3.4\S32DS\build_tools\gcc_v9.2\gcc-9.2-arm32-eabi\bin\arm-none-eabi-objcopy.exe" -O binary Debug_APP\demo_s32k144.elf app.bin
```

或者使用 S32DS 编译后自动生成的 Post-build 步骤。

### 3. 烧录 Bootloader

用 J-Link / PEMicro 下载 **Debug_FLASH** 的 elf 到板子。上电后蓝绿灯交替闪烁 = Bootloader 正常。

### 4. 刷写 App（可选两种方式）

**方式 A：J-Link 直接烧**
直接烧 Debug_APP 的 elf（最快，开发调试）。

**方式 B：CANoe UDS 刷写（真实升级流程）**
- 上电 Bootloader → CANoe 开始测量 → 自动下载
- 或 App 正常运行 → 串口发 **U** → 系统复位 → Bootloader 等待 → CANoe 刷写

---

## 快速测试清单

| # | 场景 | 操作 | 预期结果 |
|---|------|------|----------|
| 1 | 空板（无 App）上电 | 烧 Bootloader，不烧 App | 蓝绿灯 500ms 交替闪，每 5 秒打印 `No valid App` |
| 2 | 有 App 上电 | Bootloader + App 都烧好 | 打印 Boot 信息 → `Jumping to App...` → 进入 App |
| 3 | App LED 指示 | 观察 LED | 蓝灯常亮，绿灯 200ms 快闪 |
| 4 | App 串口升级 | 串口发 `U` | App 打印 `Upgrade triggered...` → 复位 → Bootloader 打印 `Upgrade flag detected` |
| 5 | CANoe 自动刷写 | CANoe 开始测量 | 自动 6 步流程 → 完成后复位 → 新 App 启动 |

---

## Git 忽略规则（已在 .gitignore 中配置）

**✅ 提交：**
- `src/` 所有代码
- `board/` 配置
- `Project_Settings/Linker_Files/` 两个链接脚本
- `.project` / `.cproject`
- `app.bin` / `uds_download.can/.cbf` / `uds_frames.csv`
- `*.py` 工具脚本
- `.gitignore` / `README.md`

**🚫 忽略：**
- `Debug_FLASH/` / `Debug_APP/` / `Debug_RAM/` 编译产物
- `Debug_Configurations/` 调试配置
- `.settings/` IDE 偏好
- `SDK/` NXP SDK 本地链接
- `Doxygen/` / `*.mex` 生成文件

---

## 注意事项

1. **外部晶振 8 MHz**：改频率需修改 `clock_config.c` 并重新计算 SPLL 倍频。
2. **CANoe 数据数组大小**：CAPL 里 `gAppData[65536]`，如果 App 超过 64KB 需增大。
3. **App 向量表地址**：Debug_APP 链接脚本设 ORIGIN=0x00010000，Bootloader 跳转时写 `SCB->VTOR=0x00010000`，必须一致。
4. **Flash 写入对齐**：写入地址/长度必须 8 字节对齐；Bootloader 的 0x36 按每 4 字节一帧攒满 8 字节写一次，最后一块在 0x37 里补 0xFF 到 8 字节。
5. **CRC 算法一致性**：固件用 CRC-32/ISO-HDLC（查表），Python 用 `zlib.crc32(data) & 0xFFFFFFFF`，CAPL 用相同多项式 0xEDB88320，三者结果必须完全一致。
6. **跳转前最小化操作**：Jump_To_App() 只做关全局中断、设 VTOR、设 MSP、跳转；不要用 SDK 的 Deinit 函数，不要操作 NVIC ICER/ICPR，避免状态卡死。
7. **ECU Reset vs 直接跳转**：推荐走 0x11 复位（完整硬件复位循环）再跳 App，不建议在 UDS 上下文直接 Jump_To_App()，容易因残留状态卡死。

---

## 许可证

SDK 源码版权归 NXP 所有，遵循 SDK 自带许可条款。`src/` 目录下用户编写的代码遵循自由使用原则。
