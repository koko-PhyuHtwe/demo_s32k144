/**
 * @file    can.h
 * @brief   FlexCAN 驱动头文件
 * @details 基于 S32K144 FlexCAN2 的 CAN 通信模块，支持中断接收和非阻塞发送
 */

#ifndef CAN_H
#define CAN_H

#include "sdk_project_config.h"

/* CAN 实例 ID 和邮箱配置 */
#define CAN_INSTANCE        INST_FLEXCAN_CONFIG_1  /* FlexCAN2 */
#define CAN_TX_MB_IDX       0U                     /* 发送邮箱 (M0) */
#define CAN_RX_MB_IDX       1U                     /* 接收邮箱 (M1) */
#define CAN_TX_TIMEOUT_MS   1000U                  /* 发送超时 (ms) */

/* CAN 发送 ID 和数据 */
#define CAN_TX_ID           0x123U                 /* 回复 ID */

/* CAN 接收 ID 过滤 */
#define CAN_RX_ID           0x7E0U                 /* CAN 卡发送的 ID */

/**
 * @brief  CAN 初始化
 * @note   初始化 FlexCAN2，配置收发邮箱，安装回调，启动中断接收
 */
void CAN_Init(void);

/**
 * @brief  通过轮询模式发送 CAN 消息
 * @param  id:   CAN 消息 ID
 * @param  data: 数据指针（最多 8 字节）
 * @param  len:  数据长度
 */
void CAN_SendMessage(uint32_t id, const uint8_t *data, uint8_t len);

/**
 * @brief  发送回复数据（ID=0x123, 8字节数据）
 */
void CAN_SendReply(void);

#endif /* CAN_H */
