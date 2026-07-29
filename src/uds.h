/**
 * @file    uds.h
 * @brief   UDS 诊断服务头文件
 * @details 实现 ISO 14229 UDS 协议子集（会话控制 / ECU 复位 / 读 DID）
 */

#ifndef UDS_H
#define UDS_H

#include "sdk_project_config.h"

/* ==================== UDS 服务 ID (SID) ==================== */
#define UDS_SID_SESSION_CONTROL       0x10U  /* 会话控制 */
#define UDS_SID_ECU_RESET             0x11U  /* ECU 复位 */
#define UDS_SID_READ_DID              0x22U  /* 读数据标识符 */
#define UDS_SID_REQUEST_DOWNLOAD      0x34U  /* 请求下载 */
#define UDS_SID_TRANSFER_DATA         0x36U  /* 传输数据 */
#define UDS_SID_REQUEST_TRANSFER_EXIT 0x37U  /* 退出传输 */

/* ==================== UDS 否定响应码 (NRC) ==================== */
#define UDS_NRC_SERVICE_NOT_SUPPORTED       0x11U  /* 服务不支持 */
#define UDS_NRC_SUBFUNCTION_NOT_SUPPORTED   0x12U  /* 子功能不支持 */
#define UDS_NRC_INVALID_MESSAGE_LENGTH      0x13U  /* 消息长度错误 */
#define UDS_NRC_REQUEST_OUT_OF_RANGE        0x31U  /* 请求超出范围 */
#define UDS_NRC_REQUEST_SEQUENCE_ERROR      0x24U  /* 请求序列错误 */
#define UDS_NRC_WRONG_BLOCK_SEQUENCE        0x73U  /* 块序号错误 */
#define UDS_NRC_GENERAL_PROGRAMMING_FAILURE 0x72U  /* 编程失败 */

/* ==================== 会话类型 ==================== */
#define UDS_SESSION_DEFAULT      0x01U  /* 默认会话 */
#define UDS_SESSION_EXTENDED     0x03U  /* 扩展会话 */

/* ==================== 函数声明 ==================== */

/**
 * @brief  UDS 初始化
 */
void UDS_Init(void);

/**
 * @brief  UDS 主处理函数
 * @note   在主循环中调用，从 CAN 队列取数据并解析
 */
void UDS_Process(void);

#endif /* UDS_H */
