/**
 * @file    can.c
 * @brief   FlexCAN 驱动源文件
 * @details 基于 S32K144 FlexCAN2 的 CAN 通信实现（中断接收 + 非阻塞发送）
 */

#include "can.h"

/* CAN 发送信息结构体 */
static flexcan_data_info_t canTxInfo = {
    .msg_id_type = FLEXCAN_MSG_ID_STD,    /* 标准 ID */
    .data_length = 8U,                     /* 数据长度 8 字节 */
    .is_remote   = false,                  /* 非远程帧 */
};

/* CAN 接收信息结构体 */
static flexcan_data_info_t canRxInfo = {
    .msg_id_type = FLEXCAN_MSG_ID_STD,    /* 标准 ID */
    .data_length = 8U,                     /* 接收数据长度 8 字节 */
    .is_remote   = false,                  /* 非远程帧 */
};

/* 预设回复数据 */
static uint8_t canReplyData[8] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88};

/* 接收数据缓冲区（供 FLEXCAN_DRV_Receive 使用） */
static flexcan_msgbuff_t canRxBuffer;

/**
 * @brief  CAN 回调函数
 * @note   接收到数据后：1) 发送回复  2) 重新启动接收
 */
static void CAN_Callback(uint8_t instance, flexcan_event_type_t eventType,
                          uint32_t buffIdx, flexcan_state_t *flexcanState)
{
    if ((eventType == FLEXCAN_EVENT_RX_COMPLETE) && (buffIdx == CAN_RX_MB_IDX))
    {
        /* 1. 非阻塞发送回复（避免在中断中阻塞） */
        FLEXCAN_DRV_Send(CAN_INSTANCE, CAN_TX_MB_IDX, &canTxInfo,
                         CAN_TX_ID, canReplyData);

        /* 2. 重新启动接收（恢复 MB 的 RX_BUSY 状态和中断使能） */
        FLEXCAN_DRV_Receive(CAN_INSTANCE, CAN_RX_MB_IDX, &canRxBuffer);
    }
}

/**
 * @brief  CAN 初始化
 * @note   初始化 FlexCAN2，配置收发邮箱，安装回调，启动中断接收
 */
void CAN_Init(void)
{
    /* 初始化 FlexCAN2（内部自动使能 NVIC 中断） */
    FLEXCAN_DRV_Init(CAN_INSTANCE, &flexcanState2, &flexcanInitConfig2);

    /* 配置发送邮箱 (M0) */
    FLEXCAN_DRV_ConfigTxMb(CAN_INSTANCE, CAN_TX_MB_IDX, &canTxInfo, CAN_TX_ID);

    /* 配置接收邮箱 (M1)，过滤 ID = 0x7E0 */
    FLEXCAN_DRV_ConfigRxMb(CAN_INSTANCE, CAN_RX_MB_IDX, &canRxInfo, CAN_RX_ID);

    /* 安装事件回调函数 */
    FLEXCAN_DRV_InstallEventCallback(CAN_INSTANCE, CAN_Callback, NULL);

    /* 启动接收：设置 MB 状态为 RX_BUSY，使能 MB 级中断 */
    FLEXCAN_DRV_Receive(CAN_INSTANCE, CAN_RX_MB_IDX, &canRxBuffer);
}

/**
 * @brief  通过轮询模式发送 CAN 消息
 * @param  id:   CAN 消息 ID
 * @param  data: 数据指针
 * @param  len:  数据长度
 */
void CAN_SendMessage(uint32_t id, const uint8_t *data, uint8_t len)
{
    /* 更新数据长度 */
    canTxInfo.data_length = len;

    /* 阻塞式发送，超时 1000ms */
    FLEXCAN_DRV_SendBlocking(CAN_INSTANCE, CAN_TX_MB_IDX, &canTxInfo, id, data, CAN_TX_TIMEOUT_MS);
}

/**
 * @brief  发送回复数据（ID=0x123, 8字节数据）
 */
void CAN_SendReply(void)
{
    CAN_SendMessage(CAN_TX_ID, canReplyData, sizeof(canReplyData));
}
