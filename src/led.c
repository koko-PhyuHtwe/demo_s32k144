/**
 * @file    led.c
 * @brief   LED 驱动源文件
 * @details 基于 S32K144 GPIO 的 LED 控制实现
 */

#include "led.h"

/* ==================== 静态函数声明 ==================== */

/* 无静态函数，不需要声明 */

/* ==================== 公共函数实现 ==================== */

/**
 * @brief  LED 初始化
 * @note   配置 GPIO 为输出，设置初始状态：LED0 亮，LED1 灭
 */
void LED_Init(void)
{
    /* LED0 点亮（高电平有效） */
    PINS_DRV_SetPins(LED0_PORT, 1u << LED0_PIN);
    
    /* LED1 熄灭 */
    PINS_DRV_ClearPins(LED1_PORT, 1u << LED1_PIN);
}

/**
 * @brief  点亮指定 LED
 * @param  led_pin: LED 引脚号
 */
void LED_TurnOn(uint8_t led_pin)
{
    if (led_pin == LED0_PIN)
    {
        PINS_DRV_SetPins(LED0_PORT, 1u << LED0_PIN);
    }
    else if (led_pin == LED1_PIN)
    {
        PINS_DRV_SetPins(LED1_PORT, 1u << LED1_PIN);
    }
}

/**
 * @brief  熄灭指定 LED
 * @param  led_pin: LED 引脚号
 */
void LED_TurnOff(uint8_t led_pin)
{
    if (led_pin == LED0_PIN)
    {
        PINS_DRV_ClearPins(LED0_PORT, 1u << LED0_PIN);
    }
    else if (led_pin == LED1_PIN)
    {
        PINS_DRV_ClearPins(LED1_PORT, 1u << LED1_PIN);
    }
}

/**
 * @brief  翻转指定 LED 状态
 * @param  led_pin: LED 引脚号
 */
void LED_Toggle(uint8_t led_pin)
{
    if (led_pin == LED0_PIN)
    {
        PINS_DRV_TogglePins(LED0_PORT, 1u << LED0_PIN);
    }
    else if (led_pin == LED1_PIN)
    {
        PINS_DRV_TogglePins(LED1_PORT, 1u << LED1_PIN);
    }
}

/**
 * @brief  同时翻转两个 LED
 * @note   用于主循环中的心跳指示
 */
void LED_ToggleBoth(void)
{
    PINS_DRV_TogglePins(LED0_PORT, 1u << LED0_PIN);
    PINS_DRV_TogglePins(LED1_PORT, 1u << LED1_PIN);
}
