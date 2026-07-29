# S32K144 Bootloader 开发项目

基于 S32 Design Studio (S32DS.3.4) + S32K144 (Cortex-M4F) 的 CAN Bootloader 开发工程。

当前阶段：**Flash 升级链路跑通**（UDS 0x34/0x36/0x37 下载服务）。

---

## 开发阶段记录

| 阶段 | 内容 | 状态 | 提交标记 |
|------|------|------|----------|
| v0.1 | 最小系统模板（时钟 + LED + UART + CAN 基础收发） | ✅ 完成 | init-template |
| v0.2 | CAN 环形队列改造，解耦中断与业务 | ✅ 完成 | can-ring-buffer |
| v0.3 | **UDS 基础服务（0x10 / 0x11 / 0x22）** | ✅ 完成 | **uds-basic** |
| v0.4 | **Flash 驱动集成 + UDS 0x34/0x36/0x37 下载链路** | ✅ 完成 | **flash-download** |
| v0.5 | 超时保护 + 自动跳 App | ⏳ 待开发 | |

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
├── src/                               ← 用户应用代码
│   ├── main.c                         ← 主程序（初始化 + 主循环调度）
│   ├── led.h / led.c                  ← LED 驱动模块
│   ├── uart.h / uart.c                ← LPUART1 串口驱动模块
│   ├── can.h / can.c                  ← FlexCAN2 驱动（中断接收 + 环形队列）
│   ├── uds.h / uds.c                  ← UDS 诊断服务（0x10/0x11/0x22/0x34/0x36/0x37）
│   └── flash_app.h / flash_app.c      ← Flash 封装模块（初始化/擦除/写入/校验）
├── board/                             ← 板级配置（S32 Config Tools 生成）
│   ├── clock_config.c / .h            ← 时钟树配置
│   ├── pin_mux.c / .h                 ← 引脚复用配置
│   ├── peripherals_osif_1.c / .h      ← OSIF 外设配置
│   ├── peripherals_lpuart_1.c / .h    ← LPUART1 配置
│   ├── peripherals_flexcan_config_1.c / .h ← FlexCAN2 配置
│   ├── peripherals_flash_1.c / .h    ← Flash 配置（C40ASF 驱动）
│   └── sdk_project_config.h           ← SDK 统一头文件
├── SDK/                               ← NXP SDK 驱动库
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
| M0 | 发送 | 0x123 | 回复数据（UDS 正响应 / 否定响应） |
| M1 | 接收 | 0x7E0 | 接收 CAN 卡数据（ID 过滤） |

**波特率计算：**
```
TQ 总数 = 1(SYNC) + (PROPSEG+1) + (PSEG1+1) + (PSEG2+1)
         = 1 + 6 + 7 + 2 = 16
Bit Rate = 8MHz / (0+1) / 16 = 500,000 bps
采样点   = (1+6+7) / 16 = 87.5%
```

**核心改造：环形队列**
- 中断回调（ISR）只负责把收到的帧写入环形队列（FIFO），不处理业务
- 主循环调用 `UDS_Process()` 从队列取数据，交给 UDS 解析器
- 队列深度 = 16，双指针（Head/Tail）环形读写

```
CAN 硬件接收
  ↓
CAN_Callback (ISR)
  ① 写入环形队列 (canRxQueue[Head], Head++)
  ② FLEXCAN_DRV_Receive() 重启接收
  ↓
主循环 UDS_Process()
  ① CAN_RxAvailable() 检查队列
  ② CAN_GetRxFrame() 取出一帧
  ③ 解析 SID → 分发到对应服务
  ④ 构造响应 → CAN_SendMessage() 发送
```

| 函数 | 功能 |
|------|------|
| `CAN_Init()` | 初始化 FlexCAN2，配置收发邮箱，启动中断接收 |
| `CAN_SendMessage(id, data, len)` | 轮询发送 CAN 消息（阻塞） |
| `CAN_RxAvailable()` | 检查接收队列中是否有数据（1=有 / 0=无） |
| `CAN_GetRxFrame(frame)` | 从队列取出一帧数据（消费） |

### Flash 封装模块 (`flash_app.h / flash_app.c`)

基于 NXP C40ASF Flash 驱动的封装层，提供安全的 Flash 操作接口。

**Flash 地址规划：**
```
0x00000000 ┌───────────────────┐
           │  Bootloader 区域   │  约 64KB
0x00010000 ├───────────────────┤  ← App 起始地址
           │                   │
           │  App 区域         │  约 448KB
           │                   │
0x00080000 └───────────────────┘
```

**Flash 对齐规则（S32K144 P-Flash）：**

| 操作 | 地址对齐 | 大小对齐 |
|------|----------|----------|
| 擦除 | 16 字节 | 4KB（扇区大小） |
| 写入 | 8 字节 | 8 字节 |

| 函数 | 功能 |
|------|------|
| `FlashApp_Init()` | 初始化 Flash 驱动 |
| `FlashApp_Erase(addr, size)` | 擦除指定区域（自动对齐） |
| `FlashApp_Write(addr, data, size)` | 写入数据（必须 8 字节对齐） |
| `FlashApp_Verify(addr, expected, size)` | 读回校验 Flash 数据 |

**安全机制：**
- 禁止擦除 Bootloader 区域（`addr < 0x00010000` 直接返回失败）
- 写入前必须擦除（Flash 只能从 1→0）
- 所有 Flash 操作期间关中断（防止 RWW 错误）

### UDS 诊断模块 (`uds.h / uds.c`)

**当前支持的 UDS 服务：**

| SID | 服务 | 子功能 | 说明 |
|-----|------|--------|------|
| **0x10** | 会话控制 | 0x01 默认 / 0x03 扩展 | 诊断会话切换 |
| **0x11** | ECU 复位 | 0x01 硬复位 | 请求复位（暂只回响应，后续加复位逻辑） |
| **0x22** | 读 DID | 0xF189 版本号 | 读取软件版本字符串 "V1.0" |
| **0x34** | 请求下载 | - | 擦除目标扇区，准备接收数据 |
| **0x36** | 传输数据 | BlockNumber | 单帧传输 4 字节，攒满 8 字节写入 Flash |
| **0x37** | 退出传输 | - | 完成下载，处理剩余数据（补 0xFF） |

**已实现的否定响应 (NRC)：**

| NRC 值 | 含义 |
|--------|------|
| 0x11 | 服务不支持 |
| 0x12 | 子功能不支持 |
| 0x13 | 消息长度错误 |
| 0x24 | 请求序列错误（未先发 0x34 就发 0x36） |
| 0x31 | 请求超出范围（DID/地址不支持） |
| 0x72 | 编程失败（Flash 写入/擦除失败） |
| 0x73 | 块序号错误（0x36 序号不连续） |

**响应格式（单帧）：**
```
肯定响应：
  data[0]   = PCI 低4位 = 响应数据长度 (1 + payloadLen)
  data[1]   = SID + 0x40（表示这是响应）
  data[2..] = payload

否定响应：
  data[0] = 0x03          (PCI = 3字节)
  data[1] = 0x7F          (否定响应标记)
  data[2] = 原 SID
  data[3] = NRC 错误码
```

| 函数 | 功能 |
|------|------|
| `UDS_Init()` | UDS 模块初始化（当前无需额外操作） |
| `UDS_Process()` | 主循环调用：取队列数据 → 解析 SID → 分发服务 |

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
    CAN_Init();                // CAN（中断接收 + 环形队列）
    FlashApp_Init();           // Flash 驱动
    UDS_Init();                // UDS 诊断服务

    /* 主循环 */
    for (;;)
    {
        UDS_Process();         // UDS 处理（有数据才解析）
        OSIF_TimeDelay(1000);  // 延时 1 秒
        LED_ToggleBoth();      // LED 心跳闪烁
    }
}
```

---

## UDS 测试清单（已全部通过 ✅）

**测试环境：** CAN 分析仪 / CAN 卡，波特率 500 kbps，ID 过滤关闭。

### 正响应测试

| # | 发送 (ID=0x7E0) | 期望回复 (ID=0x123) | 验证内容 | 状态 |
|---|------------------|----------------------|----------|------|
| 1 | `02 10 03 00 00 00 00 00` | `06 50 03 00 32 01 F4 00` | 扩展会话：P2=50ms, P2*=500ms | ✅ |
| 2 | `02 10 01 00 00 00 00 00` | `06 50 01 00 32 01 F4 00` | 默认会话 | ✅ |
| 3 | `03 22 F1 89 00 00 00 00` | `07 62 F1 89 56 31 2E 30` | 读版本号 "V1.0" (ASCII) | ✅ |
| 4 | `02 11 01 00 00 00 00 00` | `02 51 01 00 00 00 00 00` | ECU 复位正响应（暂未执行复位） | ✅ |

### 否定响应测试

| # | 发送 (ID=0x7E0) | 期望回复 (ID=0x123) | 验证内容 | 状态 |
|---|------------------|----------------------|----------|------|
| 5 | `01 3E 00 00 00 00 00 00` | `03 7F 3E 11 00 00 00 00` | 不支持的 SID → NRC=0x11 | ✅ |
| 6 | `03 22 F1 90 00 00 00 00` | `03 7F 22 31 00 00 00 00` | 不支持的 DID → NRC=0x31 | ✅ |
| 7 | `02 10 05 00 00 00 00 00` | `03 7F 10 12 00 00 00 00` | 不支持的子功能 → NRC=0x12 | ✅ |

### 环形队列压力测试

| # | 操作 | 结果 | 状态 |
|---|------|------|------|
| 8 | 3 帧周期 1ms 连发（发送 15 帧） | 收到 15 帧回复，零丢包 | ✅ |

### Flash 下载链路测试

**测试流程：**

| # | 类型 | 发送 (ID=0x7E0) | 期望回复 (ID=0x123) | 验证内容 | 状态 |
|---|------|-----------------|----------------------|----------|------|
| 9 | 请求下载 | `07 34 00 01 00 00 00 10` | `04 74 20 04 04 00 00 00` | 擦除 0x00010000 区域 | ✅ |
| 10 | 传输#1 | `06 36 01 AA BB CC DD 00` | `02 76 01 00 00 00 00 00` | 缓存前 4 字节 | ✅ |
| 11 | 传输#2 | `06 36 02 11 22 33 44 00` | `02 76 02 00 00 00 00 00` | 攒满 8 字节 → 写入 Flash | ✅ |
| 12 | 传输#3 | `06 36 03 55 66 77 88 00` | `02 76 03 00 00 00 00 00` | 缓存前 4 字节 | ✅ |
| 13 | 传输#4 | `06 36 04 99 AA BB CC 00` | `02 76 04 00 00 00 00 00` | 攒满 8 字节 → 写入 Flash | ✅ |
| 14 | 退出传输 | `01 37 00 00 00 00 00 00` | `01 77 00 00 00 00 00 00` | 下载完成 | ✅ |
| 15 | 数据校验 | 调试器看 0x00010000 | `AA BB CC DD 11 22 33 44 55 66 77 88 99 AA BB CC` | Flash 内容正确 | ✅ |

**错误场景测试：**

| # | 类型 | 发送 (ID=0x7E0) | 期望回复 (ID=0x123) | NRC | 状态 |
|---|------|-----------------|----------------------|-----|------|
| 16 | 序列错误 | `06 36 01 AA BB CC DD 00`（无 0x34） | `03 7F 36 24 00 00 00 00` | 0x24 | ✅ |
| 17 | 块号错误 | `07 34...` → `06 36 05 AA BB CC DD 00` | `03 7F 36 73 00 00 00 00` | 0x73 | ✅ |
| 18 | 地址越界 | `07 34 00 00 00 00 00 10`（Bootloader 区域） | `03 7F 34 31 00 00 00 00` | 0x31 | ✅ |

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

### 5. UDS 通信测试

参见上方"UDS 测试清单"，用 CAN 工具发送测试帧，检查回复是否匹配。

---

## Git 保存步骤

本阶段完成后，建议执行以下命令保存（Windows PowerShell / Git Bash）：

```powershell
# 1. 进入项目目录
cd c:\Users\10608\Documents\NXP_S32_3.4\demo_s32k144

# 2. 查看当前改动
git status

# 3. 添加所有改动
git add -A

# 4. 提交（阶段标记：Flash 下载链路完成）
git commit -m "feat: v0.4 Flash 升级链路跑通 (0x34 请求下载 / 0x36 传输数据 / 0x37 退出传输)
                 - 新增 flash_app.h / flash_app.c，封装 C40ASF Flash 驱动
                 - 支持 UDS 下载流程：擦除 → 传输 → 校验
                 - 18 项测试全部通过（含错误场景测试）"

# 5. (可选) 打标签
git tag -a v0.4-flash-download -m "Flash 升级链路完成，支持通过 CAN 下载固件"

# 6. (可选) 推送到远程仓库
git push origin main
git push origin --tags
```

---

## 注意事项

1. **外部晶振**：本工程使用 8 MHz 外部晶振。如使用其他频率，需修改 `clock_config.c` 中 `soscConfig.freq` 并重新计算 SPLL 倍频。
2. **CAN 时钟**：FlexCAN2 的 PE 时钟由 `peripherals_flexcan_config_1.c` 中 `pe_clock = FLEXCAN_CLK_SOURCE_OSC` 指定（8 MHz），与 PCC 层的 `clkSrc` 无关。
3. **不要手动修改 `board/` 目录下带 "This file was generated by..." 注释的文件**，否则下次用 Config Tools 更新时会被覆盖。
4. **OSIF 模式**：当前为裸机模式。如需切换为 FreeRTOS，在预处理符号中定义 `USING_OS_FREERTOS`。
5. **CAN 接收关键**：`FLEXCAN_DRV_Receive()` 必须在 `ConfigRxMb()` 之后调用，否则 MB 状态不是 `RX_BUSY`，中断不会触发回调。回调中需再次调用以恢复接收能力。
6. **UDS PCI 字段**：单帧时 PCI 低 4 位 = 有效数据长度（SID + 参数之和），注意区分"请求长度"和"响应长度"。
7. **Flash 分区**：App 起始地址 0x00010000，擦除对齐 4KB，写入对齐 8 字节。Flash 操作期间必须关中断。

---

## 下一阶段计划

### v0.5：超时保护 + 自动跳 App
1. 配置 LPIT 定时器做 S3Server 计时（5s 超时）
2. 规则：
   - 收到 0x10 会话控制 → 重置定时器（保持 Bootloader 模式）
   - 超时未收到有效 UDS → 跳 App
   - 收到 0x11 复位 / 0x37 传输完成 → 延时 100ms 后跳 App
3. 实现 `jump_to_app()`：
   - 校验 App 栈顶是否合法
   - 重定向 VTOR 到 App 地址
   - 切换 MSP → 跳到 App 复位函数
4. 完善 0x11 ECU 复位服务（真正执行系统复位）

---

## 许可证

本项目基于 NXP S32SDK 生成，SDK 源码版权归 NXP 所有，遵循 SDK 自带许可条款。

App 代码部分（src/ 目录下用户自行编写的代码）遵循自由使用原则。
