#include "flash_app.h"
#include "peripherals_flash_1.h"
#include "interrupt_manager.h"

/* Flash 驱动配置结构体（Init 后填充，后续所有操作都用它） */
static flash_ssd_config_t flashSSDConfig;

/**
 * @brief  初始化 Flash 驱动
 */
void FlashApp_Init(void)
{
    FLASH_DRV_Init(&Flash_InitConfig0, &flashSSDConfig);
}

/**
 * @brief  擦除 Flash 指定区域
 * @param  addr  起始地址（会自动对齐到扇区边界）
 * @param  size  擦除大小（会向上取整到扇区边界）
 * @return 0=成功, 1=失败
 * @note    S32K144 P-Flash: 地址 16 字节对齐, 大小 4KB 对齐
 */
uint8_t FlashApp_Erase(uint32_t addr, uint32_t size)
{
    /* 1. 地址安全检查：不能擦 Bootloader 区域 */
    if (addr < APP_FLASH_BASE) {
        return 1U;
    }
    if ((addr + size) > APP_FLASH_END) {
        return 1U;
    }
    if (size == 0U) {
        return 1U;
    }

    /* 2. 对齐：地址 16 字节对齐，大小 4KB 对齐 */
    uint32_t alignedAddr = addr & ~0x0FU;
    uint32_t alignedSize = size;
    if ((alignedSize % 0x1000U) != 0U) {
        alignedSize = ((alignedSize / 0x1000U) + 1U) * 0x1000U;
    }

    /* 3. 关中断（擦除期间不能被打断） */
    INT_SYS_DisableIRQGlobal();

    /* 4. 执行擦除 */
    status_t status = FLASH_DRV_EraseSector(&flashSSDConfig, alignedAddr, alignedSize);

    /* 5. 开中断 */
    INT_SYS_EnableIRQGlobal();

    return (status == STATUS_SUCCESS) ? 0U : 1U;
}

/**
 * @brief  写入数据到 Flash
 * @param  addr  起始地址（必须 8 字节对齐）
 * @param  data  要写入的数据
 * @param  size  数据大小（必须 8 字节对齐）
 * @return 0=成功, 1=失败
 */
uint8_t FlashApp_Write(uint32_t addr, const uint8_t *data, uint32_t size)
{
    /* 1. 地址安全检查 */
    if (addr < APP_FLASH_BASE) {
        return 1U;
    }
    if (size == 0U) {
        return 1U;
    }

    /* 2. 对齐检查：地址和大小都必须 8 字节对齐 */
    if ((addr % FLASH_WRITE_ALIGN != 0U) || (size % FLASH_WRITE_ALIGN != 0U)) {
        return 1U;
    }

    /* 3. 关中断 */
    INT_SYS_DisableIRQGlobal();

    /* 4. 执行写入 */
    status_t status = FLASH_DRV_Program(&flashSSDConfig, addr, size, data);

    /* 5. 开中断 */
    INT_SYS_EnableIRQGlobal();

    return (status == STATUS_SUCCESS) ? 0U : 1U;
}

/**
 * @brief  校验 Flash 数据
 * @param  addr     Flash 地址
 * @param  expected 期望的数据
 * @param  size     校验长度
 * @return 0=一致, 1=不一致
 */
uint8_t FlashApp_Verify(uint32_t addr, const uint8_t *expected, uint32_t size)
{
    const uint8_t *flashPtr = (const uint8_t *)addr;

    for (uint32_t i = 0; i < size; i++) {
        if (flashPtr[i] != expected[i]) {
            return 1U;
        }
    }
    return 0U;
}
