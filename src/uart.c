/**
 * @file    uart.c
 * @brief   LPUART 驱动源文件
 * @details 基于 S32K144 LPUART1 的串口通信实现（轮询模式）
 */

#include "uart.h"

/* 启动信息字符串（纯 ASCII） */
static const uint8_t bootMsg[] =
    "\r\n========================================\r\n"
    "  S32K144 System Boot\r\n"
    "  CORE_CLK  = 80 MHz\r\n"
    "  BUS_CLK   = 40 MHz\r\n"
    "  FLASH_CLK = 20 MHz\r\n"
    "  LPUART1   = 115200 8N1\r\n"
    "========================================\r\n\r\n";

/**
 * @brief  UART 初始化
 * @note   调用 LPUART_DRV_Init() 初始化 LPUART1
 */
void UART_Init(void)
{
    LPUART_DRV_Init(UART_INSTANCE, &lpUartState1, &lpuart_1_InitConfig0);
}

/**
 * @brief  通过轮询模式发送数据
 * @param  data: 数据指针
 * @param  len:  数据长度
 */
void UART_SendData(const uint8_t *data, uint32_t len)
{
    LPUART_DRV_SendDataPolling(UART_INSTANCE, data, len);
}

/**
 * @brief  发送启动信息
 */
void UART_SendBootMessage(void)
{
    UART_SendData(bootMsg, sizeof(bootMsg) - 1U);
}
