/**
 * @file convert_manager.h
 * @brief 视频格式转换管理头文件
 * 
 * 定义视频格式转换的操作接口和管理结构
 * 支持多种格式转换器的注册（YUV->RGB, MJPEG->RGB, RGB->RGB等）
 */

#ifndef _CONVERT_MANAGER_H
#define _CONVERT_MANAGER_H

#include <config.h>
#include <video_manager.h>
#include <linux/videodev2.h>

/**
 * @struct VideoConvert
 * @brief 视频格式转换器结构体
 * 
 * 定义一种格式转换器的操作接口
 * 支持多种转换器的注册和自动选择
 */
typedef struct VideoConvert{
    char *name;
    int (*isSupport)(int iPixelFormatIn, int iPixelFormatOut);
    int (*Convert)(PT_VideoBuf ptVideoBufIn, PT_VideoBuf ptVideoBufOut);
    int (*ConvertExit)(PT_VideoBuf ptVideoBufOut);
    struct VideoConvert *ptNext;
}T_VideoConvert,*PT_VideoConvert;

/* 根据输入输出格式获取合适的转换器 */
PT_VideoConvert GetVideoConvertForFormats(int iPixelFormatIn, int iPixelFormatOut);

/* 初始化视频格式转换模块 */
int VideoConvertInit(void);

/* 各个转换器的初始化函数 */
int Yuv2RgbInit(void);      /* YUV转RGB转换器初始化 */
int Mjpeg2RgbInit(void);    /* MJPEG转RGB转换器初始化 */
int Rgb2RgbInit(void);      /* RGB转RGB转换器初始化 */

/* 注册视频格式转换器 */
int RegisterVideoConvert(PT_VideoConvert ptVideoConvert);


#endif /* _CONVERT_MANAGER_H */