#ifndef FLASH_APP_H
#define FLASH_APP_H

#include "sdk_project_config.h"

/* ==================== Flash 地址规划 ==================== */
/* Bootloader 占用 0x00000000 ~ 0x0000FFFF (64KB) */
/* App 占用     0x00010000 ~ 0x0007FFFF (448KB) */
#define APP_FLASH_BASE      0x00010000U  /* App 起始地址 */
#define APP_FLASH_SIZE      0x00070000U  /* App 区域大小 (448KB) */
#define APP_FLASH_END       (APP_FLASH_BASE + APP_FLASH_SIZE)

/* ==================== Flash 对齐常量 ==================== */
#define FLASH_SECTOR_SIZE   0x1000U    /* 扇区大小 4KB (S32K144 P-Flash) */
#define FLASH_WRITE_ALIGN   8U          /* 写入对齐 8 字节 */

/* ==================== 函数声明 ==================== */
void    FlashApp_Init(void);
uint8_t FlashApp_Erase(uint32_t addr, uint32_t size);
uint8_t FlashApp_Write(uint32_t addr, const uint8_t *data, uint32_t size);
uint8_t FlashApp_Verify(uint32_t addr, const uint8_t *expected, uint32_t size);
uint32_t FlashApp_CalcCRC32(uint32_t addr, uint32_t size);

#endif /* FLASH_APP_H */
