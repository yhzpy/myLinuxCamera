/**
 * @file disp_manager.h
 * @brief 显示设备管理头文件
 * 
 * 定义显示设备（如framebuffer）的操作接口和管理结构
 * 支持多种显示设备的注册和切换
 */

#ifndef _DISP_MANAGER_H
#define _DISP_MANAGER_H

#include <pic_operation.h>
#include <video_manager.h>

/**
 * @struct Layout
 * @brief 布局结构体，定义显示区域的左上角和右下角坐标
 * 
 * 用于指定图像在显示区域中的位置和大小
 * 可以存储图标名称用于界面布局
 */
typedef struct Layout{
    int iTopLeftX;
    int iTopLeftY;
    int iBottomRightX;
    int iBottomRightY;
    char *strIconName;
}T_Layout,*PT_Layout;

/**
 * @enum E_VideoMemState
 * @brief VideoMem状态枚举
 * 
 * 描述VideoMem的当前使用状态：
 * - FREE: 空闲，可分配
 * - USED_FOR_PREPARE: 正在准备显示数据
 * - USED_FOR_CUR: 当前正在显示
 */
typedef enum {
    VMS_FREE=0,
    VMS_USED_FOR_PREPARE,
    VMS_USED_FOR_CUR
}E_VideoMemState;

/**
 * @enum E_PicState
 * @brief VideoMem中图像状态枚举
 * 
 * 描述VideoMem中图像数据的生成状态：
 * - BLANK: 空白，无有效图像
 * - GENERATING: 正在生成图像
 * - GENERATED: 图像已生成完毕
 */
typedef enum {
    PS_BLANK=0,
    PS_GENERATING,
    PS_GENERATED
}E_PicState;

/**
 * @struct VideoMem
 * @brief 视频内存结构体
 * 
 * 用于加速显示的缓冲机制：
 * - 先在内存中准备图像数据
 * - 再一次性复制到显示设备的framebuffer
 * 支持多页缓冲和链表管理
 */
typedef struct VideoMem{
    int iID;
    int bDevFrameBuffer;
    E_VideoMemState eVideoMemState;  /* VideoMem的使用状态 */
    E_PicState ePicState; /* VideoMem中图像的状态 */
    T_PixelDatas tPixelDatas;
    struct VideoMem *ptNext;
}T_VideoMem,*PT_VideoMem;

/**
 * @struct DispOpr
 * @brief 显示操作接口结构体
 * 
 * 定义显示设备的操作方法，支持多种显示驱动的注册
 */
typedef struct DispOpr{
    char *name;
    int iXres;
    int iYres;
    int iBpp;
    int iLineWidth;
    unsigned char *pucDispMem;
    int (*DeviceInir)(void);
    int (*ShowPixel)(int iPenX, int iPenY, unsigned int dwColor);    /* 在指定坐标画点 */
    int (*CleanScreen)(unsigned int dwBackColor);                    /* 清屏为指定颜色 */
    int (*ShowPage)(PT_PixelDatas ptPixelDatas);                     /* 显示一页数据 */
    struct DispOpr *ptNext;     /* 链表指针 */
}T_DispOpr,*PT_DispOpr;

/**********************************************************************
 * 函数名称: RegisterDispOpr
 * 功能描述: 注册"显示模块"，支持多种显示设备的操作接口注册
 * 输入参数: ptDispOpr - 一个结构体指针，含显示设备的操作接口
 * 输出参数: 无
 * 返 回 值: 0 - 成功, 其他值 - 失败
 ***********************************************************************/
int RegisterDispOpr(PT_DispOpr ptDispOpr);

/**********************************************************************
 * 函数名称: ShowDispOpr
 * 功能描述: 显示所有支持的"显示模块"列表
 * 输入参数: 无
 * 输出参数: 无
 * 返 回 值: 无
 ***********************************************************************/
void ShowDispOpr(void);

/**********************************************************************
 * 函数名称: DisplayInit
 * 功能描述: 注册所有显示设备
 * 输入参数: 无
 * 输出参数: 无
 * 返 回 值: 0 - 成功, 其他值 - 失败
 ***********************************************************************/
int DisplayInit(void);

/**********************************************************************
 * 函数名称: SelectAndInitDefaultDispDev
 * 功能描述: 根据名称获取指定的"显示模块"，执行初始化操作，清空屏幕
 * 输入参数: name - 显示设备名称
 * 输出参数: 无
 * 返 回 值: 无
 ***********************************************************************/
void SelectAndInitDefaultDispDev(char *name);

/**********************************************************************
 * 函数名称: GetDefaultDispDev
 * 功能描述: 获取经SelectAndInitDefaultDispDev选择的显示模块，
 *           返回该显示模块的结构体指针
 * 输入参数: 无
 * 输出参数: 无
 * 返 回 值: 显示模块的PT_DispOpr结构体指针
 ***********************************************************************/
PT_DispOpr GetDefaultDispDev(void);

/**********************************************************************
 * 函数名称: GetDispResolution
 * 功能描述: 获取当前使用的显示设备的分辨率和BPP
 * 输入参数: 无
 * 输出参数: piXres - 存放X分辨率
 *           piYres - 存放Y分辨率
 *           piBpp  - 存放BPP
 * 返 回 值: 0  - 成功
 *           -1 - 失败(未使用SelectAndInitDefaultDispDev选择显示模块)
 ***********************************************************************/
int GetDispResolution(int *piXres, int *piYres, int *piBpp);

/**********************************************************************
 * 函数名称: AllocVideoMem
 * 功能描述: 分配VideoMem: 为加速显示，在内存中预先分配多页用于显示的页面缓冲
 *           (这些缓冲称为VideoMem)
 *           显示时再把VideoMem中的数据复制到设备显存
 * 输入参数: iNum - 要分配的VideoMem数量
 * 输出参数: 无
 * 返 回 值: 0  - 成功
 *           -1 - 失败(未使用SelectAndInitDefaultDispDev选择显示模块)
 ***********************************************************************/
int AllocVideoMem(int iNum);

/**********************************************************************
 * 函数名称: GetDevVideoMem
 * 功能描述: 获取显示设备的显存VideoMem，在该显存上操作可直接在LCD上显示
 * 输入参数: 无
 * 输出参数: 无
 * 返 回 值: 显存对应的VideoMem结构体指针
 ***********************************************************************/
PT_VideoMem GetDevVideoMem(void);

/**********************************************************************
 * 函数名称: GetVideoMem
 * 功能描述: 获取一个可操作的VideoMem(在内存存储要显示的数据), 
 *           使用完后用PutVideoMem释放
 * 输入参数: iID  - ID值，先在多个VideoMem中找到ID值相同的
 *           bCur - 1: 表示当前线程需要使用VideoMem，无法等待必须要分配一个新的VideoMem
 *                  0: 表示为了改进性能而提前获取VideoMem，不是必须的
 * 输出参数: 无
 * 返 回 值: NULL   - 失败,没有可用的VideoMem
 *           非NULL - 成功,返回PT_VideoMem结构体
 ***********************************************************************/
PT_VideoMem GetVideoMem(int iID, int bCur);

/**********************************************************************
 * 函数名称: PutVideoMem
 * 功能描述: 释放使用GetVideoMem获取的VideoMem
 * 输入参数: ptVideoMem - 使用完毕的VideoMem
 * 输出参数: 无
 * 返 回 值: 无
 ***********************************************************************/
void PutVideoMem(PT_VideoMem ptVideoMem);

/**********************************************************************
 * 函数名称: ClearVideoMem
 * 功能描述: 将VideoMem的内存全部设置为某一种颜色
 * 输入参数: ptVideoMem - VideoMem结构体指针，含要清空的内存
 *           dwColor    - 清空为什么颜色
 * 输出参数: 无
 * 返 回 值: 无
 ***********************************************************************/
void ClearVideoMem(PT_VideoMem ptVideoMem, unsigned int dwColor);

/**********************************************************************
 * 函数名称: ClearVideoMemRegion
 * 功能描述: 将VideoMem的内存中指定区域全部设置为某一种颜色
 * 输入参数: ptVideoMem - VideoMem结构体指针，含要清空的内存
 *           ptLayout   - 布局结构，指定左上角和右下角的坐标
 *           dwColor    - 清空为什么颜色
 * 输出参数: 无
 * 返 回 值: 无
 ***********************************************************************/
void ClearVideoMemRegion(PT_VideoMem ptVideoMem, PT_Layout ptLayout, unsigned int dwColor);

/**********************************************************************
 * 函数名称: FBInit
 * 功能描述: 注册"framebuffer显示设备"
 * 输入参数: 无
 * 输出参数: 无
 * 返 回 值: 0 - 成功, 其他值 - 失败
 ***********************************************************************/
int FBInit(void);

/* 获取用于显示的帧缓冲区 */
int GetVideoBufForDisplay(PT_VideoBuf ptFrameBuf);

/* 将像素数据刷新到显示设备 */
void FlushPixelDatasToDev(PT_PixelDatas ptPixelDatas);

#endif /* _DISP_MANAGER_H */
