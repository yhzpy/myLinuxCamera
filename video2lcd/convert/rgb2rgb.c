/**
 * @file rgb2rgb.c
 * @brief RGB转RGB格式转换器实现文件
 * 
 * 实现RGB565到RGB565/RGB32的格式转换
 * 主要用于摄像头输出RGB565格式，而显示需要RGB32格式的情况
 */

#include <convert_manager.h>
#include <stdlib.h>
#include <string.h>

/**
 * @brief 判断是否支持RGB转RGB的格式转换
 * 
 * @param iPixelFormatIn 输入像素格式
 * @param iPixelFormatOut 输出像素格式
 * @return 1 - 支持, 0 - 不支持
 */
static int isSupportRgb2Rgb(int iPixelFormatIn, int iPixelFormatOut)
{
    if (iPixelFormatIn != V4L2_PIX_FMT_RGB565)
        return 0;
    if ((iPixelFormatOut != V4L2_PIX_FMT_RGB565) && (iPixelFormatOut != V4L2_PIX_FMT_RGB32))
    {
        return 0;
    }
    return 1;
}

/**
 * @brief RGB转RGB像素格式转换函数
 *
 * 将输入视频缓冲区中的RGB565像素数据转换为输出缓冲区指定的目标格式。
 * 目前支持两种转换路径：
 *   - RGB565 -> RGB565：直接内存拷贝，无需逐像素计算
 *   - RGB565 -> RGB32：逐像素提取RGB分量并扩展为32位0x00RRGGBB格式
 *
 * @param ptVideoBufIn  [输入] 源视频缓冲区指针，须包含RGB565格式的像素数据
 * @param ptVideoBufOut [输出] 目标视频缓冲区指针，转换结果将写入其像素数据区域；
 *                       若其像素数据缓冲区为空，则自动分配所需内存
 *
 * @return 0  转换成功
 * @return -1 不支持的输入/输出像素格式
 */
static int Rgb2RgbConvert(PT_VideoBuf ptVideoBufIn, PT_VideoBuf ptVideoBufOut)
{   
    PT_PixelDatas ptPixelDatasIn  = &ptVideoBufIn->tPixelDatas;
    PT_PixelDatas ptPixelDatasOut = &ptVideoBufOut->tPixelDatas;

    int x, y;
    int r, g, b;
    int color;
    unsigned short *pwSrc = (unsigned short *)ptPixelDatasIn->aucPixelDatas;
    unsigned int *pdwDest;

    if (ptVideoBufIn->iPixelFormat != V4L2_PIX_FMT_RGB565)
    {
        return -1;
    }

    if (ptVideoBufOut->iPixelFormat == V4L2_PIX_FMT_RGB565)
    {
        /* ---- RGB565 -> RGB565: 直接复制数据 ---- */
        ptPixelDatasOut->iWidth  = ptPixelDatasIn->iWidth;
        ptPixelDatasOut->iHeight = ptPixelDatasIn->iHeight;
        ptPixelDatasOut->iBpp    = 16;
        ptPixelDatasOut->iLineBytes  = ptPixelDatasOut->iWidth * ptPixelDatasOut->iBpp / 8;
        ptPixelDatasOut->iTotalBytes = ptPixelDatasOut->iLineBytes * ptPixelDatasOut->iHeight;

        /* 若输出缓冲区尚未分配，则按计算所得总字节数申请内存 */
        if (!ptPixelDatasOut->aucPixelDatas)
        {
            ptPixelDatasOut->aucPixelDatas = malloc(ptPixelDatasOut->iTotalBytes);
        }
        
        memcpy(ptPixelDatasOut->aucPixelDatas, ptPixelDatasIn->aucPixelDatas, ptPixelDatasOut->iTotalBytes);
        return 0;
    }
    else if (ptVideoBufOut->iPixelFormat == V4L2_PIX_FMT_RGB32)
    {
        /* ---- RGB565 -> RGB32: 需要格式转换 ---- */
        ptPixelDatasOut->iWidth  = ptPixelDatasIn->iWidth;
        ptPixelDatasOut->iHeight = ptPixelDatasIn->iHeight;
        ptPixelDatasOut->iBpp    = 32;
        ptPixelDatasOut->iLineBytes  = ptPixelDatasOut->iWidth * ptPixelDatasOut->iBpp / 8;
        ptPixelDatasOut->iTotalBytes = ptPixelDatasOut->iLineBytes * ptPixelDatasOut->iHeight;

        /* 若输出缓冲区尚未分配，则按计算所得总字节数申请内存 */
        if (!ptPixelDatasOut->aucPixelDatas)
        {
            ptPixelDatasOut->aucPixelDatas = malloc(ptPixelDatasOut->iTotalBytes);
        }

        pdwDest = (unsigned int *)ptPixelDatasOut->aucPixelDatas;//让指针一次跳过四字节，即跳过32位 rgb32
        
        /* 逐像素遍历，将每个RGB565像素转换为RGB32格式 */
        for (y = 0; y < ptPixelDatasOut->iHeight; y++)
        {
            for (x = 0; x < ptPixelDatasOut->iWidth; x++)
            {
                color = *pwSrc++;

                /* 从RGB565格式中分离出R、G、B三个分量 */
                r = color >> 11;
                g = (color >> 5) & (0x3f);
                b = color & 0x1f;

                /* 将各分量扩展至8位精度，拼装为0x00RRGGBB的32位格式并写入目标缓冲区 */
                color = ((r << 3) << 16) | ((g << 2) << 8) | (b << 3);

                *pdwDest = color;
                pdwDest++;
            }
        }
        return 0;
    }

    return -1;
}

/**
 * @brief 释放RGB转RGB转换缓冲区
 * 
 * @param ptVideoBufOut 输出视频缓冲区
 * @return 0 - 成功
 */
static int Rgb2RgbConvertExit(PT_VideoBuf ptVideoBufOut)
{
    if (ptVideoBufOut->tPixelDatas.aucPixelDatas)
    {
        free(ptVideoBufOut->tPixelDatas.aucPixelDatas);
        ptVideoBufOut->tPixelDatas.aucPixelDatas = NULL;
    }
    return 0;
}

/* 定义RGB转RGB转换器结构体 */
static T_VideoConvert g_tRgb2RgbConvert = {
    .name        = "rgb2rgb",
    .isSupport   = isSupportRgb2Rgb,
    .Convert     = Rgb2RgbConvert,
    .ConvertExit = Rgb2RgbConvertExit,
};


/* 注册RGB转RGB转换器 */
int Rgb2RgbInit(void)
{
    return RegisterVideoConvert(&g_tRgb2RgbConvert);
}
