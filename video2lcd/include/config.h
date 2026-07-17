/**
 * @file config.h
 * @brief 项目配置文件
 * 
 * 定义全局配置宏和设备路径
 */

#ifndef _CONFIG_H
#define _CONFIG_H

#include <stdio.h>

/* 调试输出宏，取消注释可关闭调试输出 */
//#define DBG_PRINTF(...)  
#define DBG_PRINTF printf

/* Framebuffer设备路径 */
#define FB_DEVICE_NAME "/dev/fb0"

#endif /* _CONFIG_H */
