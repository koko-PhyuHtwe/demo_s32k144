#include "sdk_project_config.h"
#include "osif.h"          // 添加 OSIF 头文件，提供 OSIF_TimeDelay

/* ===== 根据你的硬件实际连接，直接定义引脚 ===== */
#define PCC_CLOCK   PCC_PORTD_CLOCK    /* PTD15/16 属于 PORTD */
#define LED0_PORT   PTD
#define LED0_PIN    15
#define LED1_PORT   PTD
#define LED1_PIN    16
/* ============================================ */

int main(void)
{


    /* 1. 初始化时钟 */
    CLOCK_DRV_Init(&clockMan1_InitConfig0);

    /* 2. 初始化引脚（注意：pin_mux.c 里必须把 PTD15 和 PTD16 配成 GPIO 输出） */
    PINS_DRV_Init(NUM_OF_CONFIGURED_PINS0, g_pin_mux_InitConfigArr0);

    /* 3. 初始状态：LED0 亮，LED1 灭（假设高电平点亮；若低电平点亮，把 Set/Clear 对调） */
    PINS_DRV_SetPins(LED0_PORT, 1u << LED0_PIN);
    PINS_DRV_ClearPins(LED1_PORT, 1u << LED1_PIN);

    for (;;)
    {
        OSIF_TimeDelay(1000);               // 延时 1000 毫秒，闪烁频率稳定，肉眼舒适
        PINS_DRV_TogglePins(LED0_PORT, 1u << LED0_PIN);
        PINS_DRV_TogglePins(LED1_PORT, 1u << LED1_PIN);
    }
}
