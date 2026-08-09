#ifndef VIDEO_MANAGER_H
#define VIDEO_MANAGER_H
#include <config.h>
#include <pic_operation.h>

struct VideoDevice;
struct VideoOpr;

/* 前置定义, 供VideoDevice结构体引用 */
typedef struct VideoOpr T_VideoOpr, *PT_VideoOpr;

/*
 * VideoBuf结构体 - 视频帧数据
 * 作用: 封装从摄像头获取的一帧图像数据, 包含像素数据和格式信息,
 *       是视频帧在系统内传递的统一载体
 */
typedef struct VideoBuf{
    T_PixelDatas tPixelDatas;
    int iPixelFormat;
}T_VideoBuf, *PT_VideoBuf;

/*
 * VideoDevice结构体 - 摄像头设备描述
 * 作用: 封装一个V4L2摄像头设备的完整状态信息, 包括设备文件描述符、
 *       像素格式、分辨率、缓冲区数组等, 是视频采集的核心数据结构
 */
typedef struct VideoDevice{
    int iFd;
    int iPixelFormat;
    int iWidth;
    int iHeight;

    int iVideoBufCnt;
    int iVideoBufMaxLen;
    int iVideoBufCurIndex;
    unsigned char *pucVideBuf[NB_BUFFER];

    /* 操作函数 */
    PT_VideoOpr ptOPr;    
}T_VideoDevice, *PT_VideoDevice;

/*
 * VideoOpr结构体 - 摄像头操作接口
 * 作用: 抽象摄像头设备的通用操作接口, 不同的摄像头驱动(如V4L2)
 *       只需实现这些函数指针并注册, 即可被上层统一调用
 * 设计模式: C语言面向对象设计, 通过函数指针实现接口抽象与多态
 */
struct VideoOpr{
    char *name;
    int (*InitDevice)(char *strDevName, PT_VideoDevice ptVideoDevice);
    int (*ExitDevice)(PT_VideoDevice ptVideoDevice);
    int (*GetFrame)(PT_VideoDevice ptVideoDevice, PT_VideoBuf ptVideoBuf);
    int (*PutFrame)(PT_VideoDevice ptVideoDevice, PT_VideoBuf ptVideoBuf);
    int (*StartDevice)(PT_VideoDevice ptVideoDevice);
    int (*StopDevice)(PT_VideoDevice ptVideoDevice);
    struct VideoOpr *ptNext;
};

int V4l2Init(void);

/* 注册/查找摄像头操作接口 */
int RegisterVideoOpr(PT_VideoOpr ptVideoOpr);
PT_VideoOpr getVideoOpr(char *pcName);

/* 初始化摄像头设备(选择v4l2驱动并调用其InitDevice) */
int VideoDeviceInit(char *pcDevName, PT_VideoDevice ptVideoDevice);

#endif // VIDEO_MANAGER_H