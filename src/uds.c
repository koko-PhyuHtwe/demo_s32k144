/**
 * @file    uds.c
 * @brief   UDS 诊断服务源文件
 * @details 实现 ISO 14229 UDS 协议子集（会话控制 / ECU 复位 / 读 DID）
 *          仅支持单帧传输（SingleFrame），最大 7 字节数据
 */

#include "uds.h"
#include "can.h"
#include "flash_app.h"
#include "uart.h"
#include "osif.h"
#include "interrupt_manager.h"
#include "device_registers.h"

/* ==================== CMSIS 兼容函数（NXP SDK 不提供，自己实现） ==================== */

/**
 * @brief  设置主栈指针 MSP
 * @note   NXP S32 SDK 的 s32_core_cm4.h 不提供 __set_MSP()，
 *         这里用内联汇编实现（Cortex-M4: msr msp, r0）
 */
__attribute__((always_inline)) static inline void __set_MSP(uint32_t topOfMainStack)
{
    __asm volatile ("msr msp, %0" : : "r" (topOfMainStack) : );
}

/**
 * @brief  触发系统复位（NVIC System Reset）
 * @note   参考 S32 SDK 的 system_S32K144.c::SystemSoftwareReset() 实现
 *         通过写 SCB->AIRCR 的 SYSRESETREQ 位触发芯片复位
 */
static void NVIC_SystemReset(void)
{
    uint32_t regValue;

    /* 关中断，防止复位过程中被中断打断 */
    INT_SYS_DisableIRQGlobal();

    /* 读当前 AIRCR */
    regValue = S32_SCB->AIRCR;

    /* 清掉 VECTKEY 字段（高 16 位） */
    regValue &= ~(S32_SCB_AIRCR_VECTKEY_MASK);

    /* 写 VECTKEY = 0x05FA（必须，否则写不进去） */
    regValue |= S32_SCB_AIRCR_VECTKEY(FEATURE_SCB_VECTKEY);

    /* 置位 SYSRESETREQ（请求系统复位） */
    regValue |= S32_SCB_AIRCR_SYSRESETREQ(0x1U);

    /* 写回 AIRCR，复位立即生效 */
    S32_SCB->AIRCR = regValue;

    /* 死等：复位真正生效前不要往下走 */
    __asm volatile ("dsb 0xF" ::: "memory");
    while (1) {
        __asm volatile ("nop");
    }
}

/* ==================== 内部函数声明 ==================== */
static void UDS_HandleSessionControl(const can_frame_t *frame);
static void UDS_HandleECUReset(const can_frame_t *frame);
static void UDS_HandleReadDID(const can_frame_t *frame);
static void UDS_HandleWriteDID(const can_frame_t *frame);
static void UDS_HandleRequestDownload(const can_frame_t *frame);
static void UDS_HandleTransferData(const can_frame_t *frame);
static void UDS_HandleRequestTransferExit(const can_frame_t *frame);
static void UDS_SendPositiveResponse(uint8_t sid, const uint8_t *payload, uint8_t payloadLen);
static void UDS_SendNegativeResponse(uint8_t sid, uint8_t nrc);

/* ==================== 下载状态管理 ==================== */
typedef enum {
    DOWNLOAD_IDLE,         /* 空闲，没有下载 */
    DOWNLOAD_IN_PROGRESS    /* 下载进行中 */
} download_state_t;

static download_state_t downloadState     = DOWNLOAD_IDLE;
static uint32_t downloadStartAddr          = 0U;   /* 下载起始地址（保存供 CRC 校验） */
static uint32_t downloadAddr              = 0U;   /* 当前写入地址 */
static uint32_t downloadTotalSize         = 0U;   /* 总大小 */
static uint8_t  blockCounter              = 1U;    /* 期望的块序号 */
static uint8_t  dataBuffer[8];                    /* 数据缓存（攒8字节写一次） */
static uint8_t  dataBufferIndex           = 0U;   /* 缓存当前字节数 */

/* ==================== S3Server 超时管理 ==================== */
static volatile uint32_t s3ServerTimer = 0U;  /* 剩余超时时间 (ms) */
static uint8_t bootloaderActive = 0U;          /* 0=可以跳App, 1=正在升级，禁止跳 */

/* ==================== 公共接口 ==================== */

/**
 * @brief  UDS 初始化
 */
void UDS_Init(void)
{
    s3ServerTimer = S3_SERVER_TIMEOUT_MS;  /* 启动 S3Server 倒计时 */
    bootloaderActive = 0U;
}

/**
 * @brief  UDS 主处理函数
 * @note   在主循环中调用，从 CAN 队列取数据并解析
 */
void UDS_Process(void)
{
    /* 队列为空则直接返回 */
    if (!CAN_RxAvailable()) {
        return;
    }

    /* 取一帧数据 */
    can_frame_t frame;
    CAN_GetRxFrame(&frame);

    /* 校验：ID 必须是 0x7E0，DLC 至少 2 字节（PCI + SID） */
    if ((frame.id != CAN_RX_ID) || (frame.dlc < 2U)) {
        return;
    }

    /* 收到有效 UDS 请求，重置 S3Server 定时器 */
    s3ServerTimer = S3_SERVER_TIMEOUT_MS;

    /* 解析 SID（data[1]），分发到对应服务 */
    uint8_t sid = frame.data[1];

    switch (sid) {
        case UDS_SID_SESSION_CONTROL:
            UDS_HandleSessionControl(&frame);
            break;

        case UDS_SID_ECU_RESET:
            UDS_HandleECUReset(&frame);
            break;

        case UDS_SID_READ_DID:
            UDS_HandleReadDID(&frame);
            break;

        case UDS_SID_WRITE_DID:
            UDS_HandleWriteDID(&frame);
            break;

        case UDS_SID_REQUEST_DOWNLOAD:
            UDS_HandleRequestDownload(&frame);
            break;

        case UDS_SID_TRANSFER_DATA:
            UDS_HandleTransferData(&frame);
            break;

        case UDS_SID_REQUEST_TRANSFER_EXIT:
            UDS_HandleRequestTransferExit(&frame);
            break;

        default:
            /* 不支持的服务 */
            UDS_SendNegativeResponse(sid, UDS_NRC_SERVICE_NOT_SUPPORTED);
            break;
    }
}

/* ==================== 服务处理实现 ==================== */

/**
 * @brief  处理 0x10 会话控制
 * @note   请求：02 10 03
 *         正响应：06 50 03 00 32 01 F4
 *         （P2=50ms, P2*=500ms）
 */
static void UDS_HandleSessionControl(const can_frame_t *frame)
{
    uint8_t pci     = frame->data[0];
    uint8_t subFunc = frame->data[2];

    /* 校验长度：PCI 低 4 位 = 2（SID + 子功能 = 2 字节） */
    if ((pci & 0x0FU) != 2U) {
        UDS_SendNegativeResponse(UDS_SID_SESSION_CONTROL, UDS_NRC_INVALID_MESSAGE_LENGTH);
        return;
    }

    /* 只支持默认会话和扩展会话 */
    if ((subFunc != UDS_SESSION_DEFAULT) && (subFunc != UDS_SESSION_EXTENDED)) {
        UDS_SendNegativeResponse(UDS_SID_SESSION_CONTROL, UDS_NRC_SUBFUNCTION_NOT_SUPPORTED);
        return;
    }

    /* 构造正响应负载：子功能 + P2(2字节) + P2*(2字节) */
    uint8_t payload[5];
    payload[0] = subFunc;   /* 子功能 */
    payload[1] = 0x00U;     /* P2 高字节 */
    payload[2] = 0x32U;     /* P2 低字节 = 50ms */
    payload[3] = 0x01U;     /* P2* 高字节 */
    payload[4] = 0xF4U;     /* P2* 低字节 = 500ms */

    UDS_SendPositiveResponse(UDS_SID_SESSION_CONTROL, payload, 5);
}

/**
 * @brief  处理 0x11 ECU 复位
 * @note   请求：02 11 01
 *         正响应：02 51 01
 *         （先回复，再复位。复位逻辑后续实现）
 */
static void UDS_HandleECUReset(const can_frame_t *frame)
{
    uint8_t pci     = frame->data[0];
    uint8_t subFunc = frame->data[2];

    /* 校验长度 */
    if ((pci & 0x0FU) != 2U) {
        UDS_SendNegativeResponse(UDS_SID_ECU_RESET, UDS_NRC_INVALID_MESSAGE_LENGTH);
        return;
    }

    /* 只支持硬复位（子功能 0x01） */
    if (subFunc != 0x01U) {
        UDS_SendNegativeResponse(UDS_SID_ECU_RESET, UDS_NRC_SUBFUNCTION_NOT_SUPPORTED);
        return;
    }

    /* 正响应：子功能 */
    uint8_t payload[1];
    payload[0] = subFunc;

    UDS_SendPositiveResponse(UDS_SID_ECU_RESET, payload, 1);

    /* 延时 100ms 后执行系统硬复位 */
    OSIF_TimeDelay(100);
    NVIC_SystemReset();  /* 不会返回 */
}

/**
 * @brief  处理 0x22 读 DID
 * @note   请求：03 22 F1 89
 *         正响应：07 62 F1 89 56 31 2E 30（"V1.0"）
 */
static void UDS_HandleReadDID(const can_frame_t *frame)
{
    uint8_t pci = frame->data[0];

    /* 校验长度：PCI 低 4 位 = 3（SID + 2字节DID = 3 字节） */
    if ((pci & 0x0FU) != 3U) {
        UDS_SendNegativeResponse(UDS_SID_READ_DID, UDS_NRC_INVALID_MESSAGE_LENGTH);
        return;
    }

    /* 提取 DID（data[2] << 8 | data[3]） */
    uint16_t did = ((uint16_t)frame->data[2] << 8) | frame->data[3];

    /* 只支持 DID 0xF189（软件版本号） */
    if (did != 0xF189U) {
        UDS_SendNegativeResponse(UDS_SID_READ_DID, UDS_NRC_REQUEST_OUT_OF_RANGE);
        return;
    }

    /* 版本号字符串（ASCII "V1.0"，4 字节，单帧放得下） */
    uint8_t payload[6];
    payload[0] = (uint8_t)(did >> 8);       /* DID 高字节 */
    payload[1] = (uint8_t)(did & 0xFFU);    /* DID 低字节 */
    payload[2] = 'V';
    payload[3] = '1';
    payload[4] = '.';
    payload[5] = '0';

    UDS_SendPositiveResponse(UDS_SID_READ_DID, payload, 6);
}

/**
 * @brief  处理 0x2E 写 DID（用于 CRC32 校验）
 * @note   请求格式（DID 0xFF01）：
 *         data[0] = PCI = 0x06（6 字节数据）
 *         data[1] = 0x2E (SID)
 *         data[2] = 0xFF (DID 高字节)
 *         data[3] = 0x01 (DID 低字节)
 *         data[4] = CRC[31:24]
 *         data[5] = CRC[23:16]
 *         data[6] = CRC[15:8]
 *         data[7] = CRC[7:0]
 *
 *         正响应：04 6E FF 01 00  （末位 0=校验通过）
 *         否定响应：NRC=0x72 校验失败 / NRC=0x31 不支持的 DID
 */
static void UDS_HandleWriteDID(const can_frame_t *frame)
{
    uint8_t pci     = frame->data[0];
    uint8_t dataLen = pci & 0x0FU;

    /* 校验长度：SID + 2字节DID + 4字节CRC = 7 字节 */
    if (dataLen != 7U) {
        UDS_SendNegativeResponse(UDS_SID_WRITE_DID, UDS_NRC_INVALID_MESSAGE_LENGTH);
        return;
    }

    /* 提取 DID */
    uint16_t did = ((uint16_t)frame->data[2] << 8) | frame->data[3];

    /* 只支持 DID 0xFF01（CRC32 校验请求） */
    if (did != UDS_DID_CRC32) {
        UDS_SendNegativeResponse(UDS_SID_WRITE_DID, UDS_NRC_REQUEST_OUT_OF_RANGE);
        return;
    }

    /* 检查是否已下载完成（必须有 downloadTotalSize > 0 才能校验） */
    if (downloadState != DOWNLOAD_IDLE || downloadTotalSize == 0U) {
        UDS_SendNegativeResponse(UDS_SID_WRITE_DID, UDS_NRC_REQUEST_SEQUENCE_ERROR);
        return;
    }

    /* 提取上位机给的 CRC32（大端） */
    uint32_t expectedCrc = ((uint32_t)frame->data[4] << 24) |
                           ((uint32_t)frame->data[5] << 16) |
                           ((uint32_t)frame->data[6] << 8)  |
                           ((uint32_t)frame->data[7]);

    /* 计算 Flash 区域的 CRC32（downloadStartAddr 在 0x34 中保存） */
    uint32_t actualCrc = FlashApp_CalcCRC32(downloadStartAddr, downloadTotalSize);

    if (actualCrc != expectedCrc) {
        /* CRC 不一致，校验失败，不跳 App */
        UDS_SendNegativeResponse(UDS_SID_WRITE_DID, UDS_NRC_GENERAL_PROGRAMMING_FAILURE);
        UART_SendString("CRC FAIL\r\n");
        return;
    }

    /* CRC 一致，校验通过 */
    UART_SendString("CRC OK\r\n");

    /* 正响应：DID + status(0=pass) */
    uint8_t payload[3];
    payload[0] = (uint8_t)(did >> 8);
    payload[1] = (uint8_t)(did & 0xFFU);
    payload[2] = 0x00U;  /* status = 0 表示成功 */

    UDS_SendPositiveResponse(UDS_SID_WRITE_DID, payload, 3);

    /* CRC 校验通过，延时后跳转到 App */
    UART_SendString("Jumping to App...\r\n");
    OSIF_TimeDelay(100);
    Jump_To_App();
}

/* ==================== 下载服务实现 ==================== */

/**
 * @brief  处理 0x34 请求下载
 * @note   请求格式：
 *         data[0] = PCI
 *         data[1] = 0x34 (SID)
 *         data[2] = 地址高字节
 *         data[3] = 地址次高字节
 *         data[4] = 地址次低字节
 *         data[5] = 地址低字节
 *         data[6] = 大小高字节
 *         data[7] = 大小低字节
 *         正响应：03 74 20 04
 */
static void UDS_HandleRequestDownload(const can_frame_t *frame)
{
    uint8_t pci     = frame->data[0];
    uint8_t dataLen = pci & 0x0FU;

    /* 校验长度：至少 7 字节（SID + 4字节地址 + 2字节大小） */
    if (dataLen < 7U) {
        UDS_SendNegativeResponse(UDS_SID_REQUEST_DOWNLOAD, UDS_NRC_INVALID_MESSAGE_LENGTH);
        return;
    }

    /* 解析下载地址（4字节，大端）和大小（2字节，大端） */
    uint32_t addr = ((uint32_t)frame->data[2] << 24) |
                    ((uint32_t)frame->data[3] << 16) |
                    ((uint32_t)frame->data[4] << 8)  |
                    ((uint32_t)frame->data[5]);
    uint16_t size = ((uint16_t)frame->data[6] << 8) | frame->data[7];

    /* 地址安全检查：不能写 Bootloader 区域 */
    if (addr < APP_FLASH_BASE) {
        UDS_SendNegativeResponse(UDS_SID_REQUEST_DOWNLOAD, UDS_NRC_REQUEST_OUT_OF_RANGE);
        return;
    }
    if ((addr + size) > APP_FLASH_END) {
        UDS_SendNegativeResponse(UDS_SID_REQUEST_DOWNLOAD, UDS_NRC_REQUEST_OUT_OF_RANGE);
        return;
    }
    if (size == 0U) {
        UDS_SendNegativeResponse(UDS_SID_REQUEST_DOWNLOAD, UDS_NRC_REQUEST_OUT_OF_RANGE);
        return;
    }

    /* 擦除目标区域 */
    uint8_t result = FlashApp_Erase(addr, (uint32_t)size);
    if (result != 0U) {
        UDS_SendNegativeResponse(UDS_SID_REQUEST_DOWNLOAD, UDS_NRC_GENERAL_PROGRAMMING_FAILURE);
        return;
    }

    /* 初始化下载状态 */
    downloadState    = DOWNLOAD_IN_PROGRESS;
    downloadStartAddr = addr;   /* 保存下载起始地址供 CRC 校验 */
    downloadAddr     = addr;
    downloadTotalSize = (uint32_t)size;
    blockCounter     = 1U;
    dataBufferIndex  = 0U;
    bootloaderActive = 1U;  /* 升级中，禁止跳 App */

    /* 构造正响应：数据格式标识 + 地址长度 + 每块最大字节数 */
    uint8_t payload[3];
    payload[0] = 0x20U;  /* 数据格式：不压缩 */
    payload[1] = 0x04U; /* 地址长度=4字节 */
    payload[2] = 0x04U; /* 每块最大数据长度=4字节 */

    UDS_SendPositiveResponse(UDS_SID_REQUEST_DOWNLOAD, payload, 3);
}

/**
 * @brief  处理 0x36 传输数据
 * @note   请求格式：
 *         data[0] = PCI
 *         data[1] = 0x36 (SID)
 *         data[2] = BlockNumber (序号，从1开始)
 *         data[3..6] = 4字节数据
 *         正响应：02 76 <BlockNumber>
 */
static void UDS_HandleTransferData(const can_frame_t *frame)
{
    uint8_t pci          = frame->data[0];
    uint8_t dataLen      = pci & 0x0FU;
    uint8_t recvBlockNum = frame->data[2];

    /* 检查是否在下载状态 */
    if (downloadState != DOWNLOAD_IN_PROGRESS) {
        UDS_SendNegativeResponse(UDS_SID_TRANSFER_DATA, UDS_NRC_REQUEST_SEQUENCE_ERROR);
        return;
    }

    /* 检查块序号是否正确 */
    if (recvBlockNum != blockCounter) {
        UDS_SendNegativeResponse(UDS_SID_TRANSFER_DATA, UDS_NRC_WRONG_BLOCK_SEQUENCE);
        return;
    }

    /* 提取数据（data[3]开始，最多4字节） */
    /* 校验最小长度：至少 SID + BlockNumber = 2 字节 */
    if (dataLen < 2U) {
        UDS_SendNegativeResponse(UDS_SID_TRANSFER_DATA, UDS_NRC_INVALID_MESSAGE_LENGTH);
        return;
    }
    uint8_t recvDataLen = dataLen - 2U;  /* 减去 SID 和 BlockNumber */
    if (recvDataLen > 4U) {
        recvDataLen = 4U;
    }

    /* 把数据放入缓存 */
    for (uint8_t i = 0U; i < recvDataLen; i++) {
        dataBuffer[dataBufferIndex++] = frame->data[3U + i];
    }

    /* 攒满 8 字节，写一次 Flash */
    if (dataBufferIndex >= 8U) {
        uint8_t result = FlashApp_Write(downloadAddr, dataBuffer, 8U);
        if (result != 0U) {
            UDS_SendNegativeResponse(UDS_SID_TRANSFER_DATA, UDS_NRC_GENERAL_PROGRAMMING_FAILURE);
            downloadState = DOWNLOAD_IDLE;
            return;
        }
        downloadAddr += 8U;
        dataBufferIndex = 0U;
    }

    /* 块序号 +1（0xFF 后绕回 1） */
    blockCounter++;
    if (blockCounter == 0U) {
        blockCounter = 1U;
    }

    /* 回复正响应 */
    uint8_t payload[1];
    payload[0] = recvBlockNum;

    UDS_SendPositiveResponse(UDS_SID_TRANSFER_DATA, payload, 1);
}

/**
 * @brief  处理 0x37 退出传输
 * @note   如果缓存里还有不足 8 字节的数据，补 0xFF 后写入
 *         正响应：01 77
 */
static void UDS_HandleRequestTransferExit(const can_frame_t *frame)
{
    (void)frame;  /* 0x37 请求不需要额外参数 */

    /* 检查是否在下载状态 */
    if (downloadState != DOWNLOAD_IN_PROGRESS) {
        UDS_SendNegativeResponse(UDS_SID_REQUEST_TRANSFER_EXIT, UDS_NRC_REQUEST_SEQUENCE_ERROR);
        return;
    }

    /* 处理剩余数据（不足 8 字节，补 0xFF 到 8 字节写入） */
    if (dataBufferIndex > 0U) {
        for (uint8_t i = dataBufferIndex; i < 8U; i++) {
            dataBuffer[i] = 0xFFU;
        }
        uint8_t result = FlashApp_Write(downloadAddr, dataBuffer, 8U);
        if (result != 0U) {
            UDS_SendNegativeResponse(UDS_SID_REQUEST_TRANSFER_EXIT, UDS_NRC_GENERAL_PROGRAMMING_FAILURE);
            downloadState = DOWNLOAD_IDLE;
            return;
        }
    }

    /* 下载完成，回到空闲状态 */
    downloadState = DOWNLOAD_IDLE;
    bootloaderActive = 0U;  /* 允许跳 App */

    /* 回复正响应（无额外数据） */
    UDS_SendPositiveResponse(UDS_SID_REQUEST_TRANSFER_EXIT, NULL, 0);

    /* 升级完成，等待上位机发送 0x2E 做 CRC32 校验后再跳 App */
    UART_SendString("Upgrade done, waiting for CRC32 check...\r\n");
}

/* ==================== 内部辅助函数 ==================== */

/**
 * @brief  发送 UDS 肯定响应
 * @param  sid:        原始 SID（函数内部自动 +0x40）
 * @param  payload:    响应负载（SID 之后的全部数据，含子功能/DID等）
 * @param  payloadLen: 负载长度
 *
 * @note   CAN 单帧格式：
 *         data[0] = PCI（低 4 位 = 数据长度 = 1 + payloadLen）
 *         data[1] = SID + 0x40
 *         data[2..] = payload
 */
static void UDS_SendPositiveResponse(uint8_t sid, const uint8_t *payload, uint8_t payloadLen)
{
    uint8_t txData[8] = {0, 0, 0, 0, 0, 0, 0, 0};

    /* 数据总长度 = SID(1) + payload */
    uint8_t dataLen = payloadLen + 1U;

    /* 单帧最多 7 字节数据（PCI 1 字节 + 数据 7 字节 = 8） */
    if (dataLen > 7U) {
        dataLen = 7U;
    }

    /* PCI：单帧，低 4 位 = 数据长度 */
    txData[0] = dataLen;

    /* SID + 0x40 = 响应 SID */
    txData[1] = sid + 0x40U;

    /* 拷贝 payload */
    for (uint8_t i = 0; i < payloadLen && (i + 2U) < 8U; i++) {
        txData[i + 2U] = payload[i];
    }

    /* 发送（DLC 固定 8，不足部分补零） */
    CAN_SendMessage(CAN_TX_ID, txData, 8);
}

/**
 * @brief  发送 UDS 否定响应
 * @param  sid: 原始 SID
 * @param  nrc: 否定响应码
 *
 * @note   格式：03 7F <SID> <NRC>
 */
static void UDS_SendNegativeResponse(uint8_t sid, uint8_t nrc)
{
    uint8_t txData[8] = {0, 0, 0, 0, 0, 0, 0, 0};

    txData[0] = 0x03U;   /* PCI：3 字节数据 */
    txData[1] = 0x7FU;   /* 否定响应标记 */
    txData[2] = sid;      /* 原 SID */
    txData[3] = nrc;      /* 错误码 */

    CAN_SendMessage(CAN_TX_ID, txData, 8);
}

/* ==================== 超时与跳转 ==================== */

/**
 * @brief  UDS 定时器 tick（主循环每次调用递减 1ms）
 * @note   超时后自动跳转 App
 */
void UDS_Tick(void)
{
    if (s3ServerTimer > 0U) {
        s3ServerTimer--;
        if (s3ServerTimer == 0U) {
            /* 超时了，如果不在升级中就跳 App */
            if (!bootloaderActive) {
                UART_SendString("S3 timeout, checking App...\r\n");
                Jump_To_App();
            } else {
                /* 升级中，延长超时 */
                UART_SendString("S3 timeout, download in progress, wait...\r\n");
                s3ServerTimer = S3_SERVER_TIMEOUT_MS;
            }
        }
    }
}

/**
 * @brief  跳转到 App
 * @note   1. 检查 App 栈顶是否合法（SRAM 范围内）
 *         2. 检查 App 复位向量是否合法（App Flash 范围内）
 *         3. 重定向向量表到 App 地址
 *         4. 设置主栈指针为 App 栈顶
 *         5. 跳转到 App 的 Reset_Handler
 *         不会返回
 */
void Jump_To_App(void)
{
    /* 1. 读取 App 的栈顶地址和复位向量 */
    uint32_t appMsp = *(volatile uint32_t *)APP_FLASH_BASE;
    uint32_t appPc  = *(volatile uint32_t *)(APP_FLASH_BASE + 4U);

    /* 2. 合法性检查：
     *    - MSP 必须在 SRAM 范围内 (S32K144: 0x20000000 ~ 0x2000F000)
     *    - PC  必须在 App Flash 范围内 (0x00010000 ~ 0x0007FFFF)
     *    任一不通过就认为是非法 App（比如残留测试数据）
     */
    if ((appMsp == 0xFFFFFFFFU) || (appMsp == 0x00000000U) ||
        (appMsp < 0x20000000U) || (appMsp > 0x20010000U) ||
        (appPc  < APP_FLASH_BASE) || (appPc >= APP_FLASH_END)) {
        UART_SendString("No valid App, stay in Bootloader\r\n");
        /* 没有 App，重新开始倒计时 */
        s3ServerTimer = S3_SERVER_TIMEOUT_MS;
        /* 确保中断重新打开（如果之前被关过） */
        INT_SYS_EnableIRQGlobal();
        return;
    }

    /* 3. 关闭所有中断 */
    UART_SendString("Jumping to App...\r\n");
    INT_SYS_DisableIRQGlobal();

    /* 4. 重定向向量表到 App 地址 */
    S32_SCB->VTOR = APP_FLASH_BASE;

    /* 5. 设置主栈指针 */
    __set_MSP(appMsp);

    /* 6. 跳转到 App 的 Reset_Handler */
    ((void (*)(void))appPc)();

    /* 不会执行到这里 */
}
