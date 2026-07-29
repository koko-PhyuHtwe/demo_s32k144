/**
 * @file    uds.c
 * @brief   UDS 诊断服务源文件
 * @details 实现 ISO 14229 UDS 协议子集（会话控制 / ECU 复位 / 读 DID）
 *          仅支持单帧传输（SingleFrame），最大 7 字节数据
 */

#include "uds.h"
#include "can.h"

/* ==================== 内部函数声明 ==================== */
static void UDS_HandleSessionControl(const can_frame_t *frame);
static void UDS_HandleECUReset(const can_frame_t *frame);
static void UDS_HandleReadDID(const can_frame_t *frame);
static void UDS_SendPositiveResponse(uint8_t sid, const uint8_t *payload, uint8_t payloadLen);
static void UDS_SendNegativeResponse(uint8_t sid, uint8_t nrc);

/* ==================== 公共接口 ==================== */

/**
 * @brief  UDS 初始化
 */
void UDS_Init(void)
{
    /* 目前无需额外初始化 */
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

    /* TODO: 延时 100ms 后执行 NVIC_SystemReset() 完成硬复位 */
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
