/**
 * @file    partition.c
 * @brief   分区描述表源文件
 * @details A/B 双区元数据管理：读写 Flash 元数据区里的分区描述表，
 *          支持"写空闲槽 → 原子提交"的升级提交流程。
 */

#include "partition.h"
#include "flash_app.h"
#include <string.h>

/* ==================== RAM 描述表副本 ==================== */
static partition_table_t g_partition;

/* ==================== 公共函数实现 ==================== */

/**
 * @brief  初始化：从元数据区读取描述表
 */
void Partition_Init(void)
{
    /* 从 Flash 元数据区读取（Flash 可 memory-mapped 直接读） */
    memcpy(&g_partition, (const void *)PARTITION_TABLE_ADDR, sizeof(g_partition));

    /* magic 不匹配 → 首次烧录，初始化为默认表 */
    if (g_partition.magic != PARTITION_TABLE_MAGIC) {
        memset(&g_partition, 0, sizeof(g_partition));
        g_partition.magic        = PARTITION_TABLE_MAGIC;
        g_partition.active_slot  = (uint32_t)SLOT_A;
        g_partition.slot_a_state = SLOT_STATE_EMPTY;
        g_partition.slot_b_state = SLOT_STATE_EMPTY;
    }
}

/**
 * @brief  描述表是否有效
 */
uint8_t Partition_IsValid(void)
{
    return (g_partition.magic == PARTITION_TABLE_MAGIC) ? 1U : 0U;
}

/**
 * @brief  获取当前激活槽 ID
 */
slot_id_t Partition_GetActiveSlot(void)
{
    if (g_partition.active_slot == (uint32_t)SLOT_B) {
        return SLOT_B;
    }
    return SLOT_A;
}

/**
 * @brief  获取当前激活槽基地址
 */
uint32_t Partition_GetActiveBase(void)
{
    return (Partition_GetActiveSlot() == SLOT_A) ? SLOT_A_BASE : SLOT_B_BASE;
}

/**
 * @brief  获取当前激活槽版本号
 */
uint32_t Partition_GetActiveVersion(void)
{
    if (Partition_GetActiveSlot() == SLOT_A) {
        return g_partition.slot_a_version;
    }
    return g_partition.slot_b_version;
}

/**
 * @brief  获取非激活槽基地址（下载目标）
 */
uint32_t Partition_GetInactiveBase(void)
{
    return (Partition_GetActiveSlot() == SLOT_A) ? SLOT_B_BASE : SLOT_A_BASE;
}

/**
 * @brief  提交升级：把新固件信息写入描述表并翻转激活槽
 * @param  crc32  新固件 CRC32
 * @param  size   新固件字节数
 * @return 0=成功, 1=失败
 * @note   A/B 双区流程：
 *         1. 目标槽 = 非激活槽（新固件写空闲槽）
 *         2. 新版本号 = 当前激活槽版本 + 1
 *         3. 更新 RAM 副本中目标槽的 version/crc/size/state
 *         4. 翻转 active_slot
 *         5. 擦写元数据区落盘（原子提交）
 */
uint8_t Partition_CommitUpdate(uint32_t crc32, uint32_t size)
{
    /* 目标槽 = 非激活槽：升级永远写空闲槽，提交时原子切换 */
    slot_id_t target = (Partition_GetActiveSlot() == SLOT_A) ? SLOT_B : SLOT_A;

    /* 新版本号 = 当前激活槽版本 + 1 */
    uint32_t newVersion = Partition_GetActiveVersion() + 1U;

    /* 更新 RAM 副本中目标槽的字段 */
    if (target == SLOT_A) {
        g_partition.slot_a_version = newVersion;
        g_partition.slot_a_crc32   = crc32;
        g_partition.slot_a_size    = size;
        g_partition.slot_a_state   = SLOT_STATE_VALID;
    } else {
        g_partition.slot_b_version = newVersion;
        g_partition.slot_b_crc32   = crc32;
        g_partition.slot_b_size    = size;
        g_partition.slot_b_state   = SLOT_STATE_VALID;
    }

    /* 翻转激活槽 */
    g_partition.active_slot = (uint32_t)target;

    /* 原子落盘：擦写描述表扇区 */
    return FlashApp_WriteBootSector(PARTITION_TABLE_ADDR,
                                    (const uint8_t *)&g_partition,
                                    sizeof(g_partition));
}