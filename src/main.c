/**
 * @file    main.c
 * @brief   主程序入口
 * @details S32K144 演示程序：LED 闪烁 + UART 启动信息 + CAN 中断接收回复
 */

#include "sdk_project_config.h"
#include "osif.h"
#include "led.h"
#include "uart.h"
#include "can.h"

/**
 * @brief  主函数
 * @return 理论上不会返回
 */
int main(void)
{
    /* ===== 系统初始化 ===== */
    
    /* 1. 初始化时钟 */
    CLOCK_DRV_Init(&clockMan1_InitConfig0);
    
    /* 2. 初始化引脚 */
    PINS_DRV_Init(NUM_OF_CONFIGURED_PINS0, g_pin_mux_InitConfigArr0);
    
    /* ===== 外设初始化 ===== */
    
    /* 3. 初始化 LED */
    LED_Init();
    
    /* 4. 初始化 UART 并发送启动信息 */
    UART_Init();
    UART_SendBootMessage();
    
    /* 5. 初始化 CAN（配置发送邮箱 M0 + 接收邮箱 M1，开启中断） */
    CAN_Init();
    
    /* ===== 主循环 ===== */
    for (;;)
    {
        /* 延时 1 秒 */
        OSIF_TimeDelay(1000);
        
        /* 翻转 LED 状态（心跳指示） */
        LED_ToggleBoth();
        
        /* CAN 发送已改为中断模式：
         * CAN 卡发送 ID=0x7E0 → M1 接收 → 触发中断 → M0 回复 ID=0x123
         */
    }
}
