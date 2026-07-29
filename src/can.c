/**
 * @file    can.c
 * @brief   FlexCAN 驱动源文件
 * @details 基于 S32K144 FlexCAN2 的 CAN 通信实现（中断接收 + 非阻塞发送）
 */

#include "can.h"

/* ==================== 静态变量定义 ==================== */

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

/* 接收数据缓冲区（供 FLEXCAN_DRV_Receive 使用） */
static flexcan_msgbuff_t canRxBuffer;

/* ===== 新增：环形队列相关变量 ===== */
static can_frame_t canRxQueue[CAN_RX_QUEUE_SIZE]; /* 存储数组（货架） */
static volatile uint8_t canRxHead = 0;             /* 写指针（中断用） */
static volatile uint8_t canRxTail = 0;             /* 读指针（主循环用） */

/* ==================== 静态函数声明 ==================== */

/**
 * @brief  CAN 回调函数
 * @param  instance      CAN 实例号
 * @param  eventType     事件类型
 * @param  buffIdx       邮箱索引
 * @param  flexcanState  CAN 状态指针
 * @note   接收到数据后：1) 发送回复  2) 重新启动接收
 */
static void CAN_Callback(uint8_t instance, flexcan_event_type_t eventType,
                          uint32_t buffIdx, flexcan_state_t *flexcanState);

/* ==================== 公共函数实现 ==================== */

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
 * @param  id    CAN 消息 ID
 * @param  data  数据指针（最多 8 字节）
 * @param  len   数据长度（1-8）
 */
void CAN_SendMessage(uint32_t id, const uint8_t *data, uint8_t len)
{
    /* 更新数据长度 */
    canTxInfo.data_length = len;

    /* 阻塞式发送，超时 1000ms */
    FLEXCAN_DRV_SendBlocking(CAN_INSTANCE, CAN_TX_MB_IDX, &canTxInfo,
                             id, data, CAN_TX_TIMEOUT_MS);
}

/**
 * @brief  检查接收队列中是否有可用数据
 * @return 1: 有数据, 0: 无数据
 */
uint8_t CAN_RxAvailable(void)
{
    return (canRxHead != canRxTail);
}

/**
 * @brief  从接收队列中获取一帧数据
 * @param  frame: 用于存放读取到的帧数据
 */
void CAN_GetRxFrame(can_frame_t *frame)
{
    if (CAN_RxAvailable()) {
        *frame = canRxQueue[canRxTail];
        canRxTail = (canRxTail + 1) % CAN_RX_QUEUE_SIZE;
    }
}


/* ==================== 静态函数实现 ==================== */

/**
 * @brief  CAN 回调函数
 * @param  instance      CAN 实例号
 * @param  eventType     事件类型
 * @param  buffIdx       邮箱索引
 * @param  flexcanState  CAN 状态指针
 * @note   接收到数据后：1) 发送回复  2) 重新启动接收
 */
static void CAN_Callback(uint8_t instance, flexcan_event_type_t eventType,
                          uint32_t buffIdx, flexcan_state_t *flexcanState)
{
    /* 消除未使用参数警告 */
    (void)instance;
    (void)flexcanState;

    if ((eventType == FLEXCAN_EVENT_RX_COMPLETE) && (buffIdx == CAN_RX_MB_IDX))
    {
        /* 1. 计算下一个写位置（先不写，先看能不能写） */
        uint8_t next = (canRxHead + 1) % CAN_RX_QUEUE_SIZE;

        /* 2. 如果队列没满（next != tail），才写入 */
        if (next != canRxTail) {
            canRxQueue[canRxHead].id  = canRxBuffer.msgId;
            canRxQueue[canRxHead].dlc = canRxBuffer.dataLen;
            memcpy(canRxQueue[canRxHead].data, canRxBuffer.data, 8);
            canRxHead = next;  /* 推进写指针 */
        }
        /* 如果满了就丢弃这帧（简化处理） */

        /* 3. 重新启动接收 */
        FLEXCAN_DRV_Receive(CAN_INSTANCE, CAN_RX_MB_IDX, &canRxBuffer);
    }
}
