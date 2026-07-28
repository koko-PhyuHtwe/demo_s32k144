/**
 * @file    uart.h
 * @brief   LPUART 驱动头文件
 * @details 基于 S32K144 的 LPUART1 串口通信模块，支持轮询模式发送
 */

#ifndef UART_H
#define UART_H

#include "sdk_project_config.h"

/* 串口实例 ID */
#define UART_INSTANCE       INST_LPUART_1    /* LPUART1 */

/**
 * @brief  UART 初始化
 * @note   初始化 LPUART1，波特率 115200，8N1
 */
void UART_Init(void);

/**
 * @brief  通过轮询模式发送数据
 * @param  data: 要发送的数据缓冲区指针
 * @param  len:  数据长度（字节）
 */
void UART_SendData(const uint8_t *data, uint32_t len);

/**
 * @brief  发送启动信息（ASCII 字符）
 */
void UART_SendBootMessage(void);

#endif /* UART_H */
