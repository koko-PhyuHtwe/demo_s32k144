/**
 * @file    app.h
 * @brief   应用程序头文件
 * @details App 程序：LED 闪烁 + 串口打印 + 等待升级命令
 *          编译时定义 BUILD_APP 宏启用
 */

#ifndef APP_H
#define APP_H

#include "sdk_project_config.h"

/* ==================== App 版本 ==================== */
#define APP_VERSION  "V1.0"

/* ==================== 升级标志 ==================== */
#define UPGRADE_FLAG_ADDR     0x0003FFF0U  /* 升级标志存放地址 */
#define UPGRADE_FLAG_MAGIC    0x5AA55AA5U  /* 升级请求标志值 */

/* ==================== 函数声明 ==================== */
int app_main(void);

#endif /* APP_H */
