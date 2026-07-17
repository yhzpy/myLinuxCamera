/**
 * @file yuv2rgb.c
 * @brief YUV转RGB格式转换器实现文件
 * 
 * 实现YUV422格式到RGB格式的转换
 * 支持转换为RGB565和RGB32两种输出格式
 * 
 * 转换公式：
 * R = Y + 1.4075 * (V - 128)
 * G = Y - 0.3455 * (U - 128) - 0.7169 * (V - 128)
 * B = Y + 1.7790 * (U - 128)
 */

#include <convert_manager.h>
#include <stdlib.h>
#include "color.h"

/**
 * @brief 判断是否支持YUV转RGB的格式转换
 * 
 * @param iPixelFormatIn 输入像素格式
 * @param iPixelFormatOut 输出像素格式
 * @return 1 - 支持, 0 - 不支持
 */
static int isSupportYuv2Rgb(int iPixelFormatIn, int iPixelFormatOut)
{
    if (iPixelFormatIn != V4L2_PIX_FMT_YUYV)
        return 0;
    if ((iPixelFormatOut != V4L2_PIX_FMT_RGB565) && (iPixelFormatOut != V4L2_PIX_FMT_RGB32))
    {
        return 0;
    }
    return 1;
}



/**
 * @brief 将YUV422Packed格式转换为RGB565格式
 * 
 * YUV422格式：每4个字节表示2个像素
 * [Y0 U0 Y1 V0] -> [RGB0 RGB1]
 * 
 * @param input_ptr 输入YUV数据指针
 * @param output_ptr 输出RGB数据指针
 * @param image_width 图像宽度
 * @param image_height 图像高度
 * @return 0 - 成功
 */
static unsigned int
Pyuv422torgb565(unsigned char * input_ptr, unsigned char * output_ptr, unsigned int image_width, unsigned int image_height)
{
	unsigned int i, size;
	unsigned char Y, Y1, U, V;
	unsigned char *buff = input_ptr;
	unsigned char *output_pt = output_ptr;

    unsigned int r, g, b;
    unsigned int color;
    
	size = image_width * image_height /2;
	for (i = size; i > 0; i--) {
		/* 从YUV422格式中取出Y、U、V分量 */
		Y = buff[0] ;
		U = buff[1] ;
		Y1 = buff[2];
		V = buff[3];
		buff += 4;
		
		/* 第一个像素：YUV转RGB */
		r = R_FROMYV(Y,V);
		g = G_FROMYUV(Y,U,V);
		b = B_FROMYU(Y,U);

        /* 将r,g,b颜色转换为RGB565的16位值 */
        r = r >> 3;  /* R: 8位 -> 5位 */
        g = g >> 2;  /* G: 8位 -> 6位 */
        b = b >> 3;  /* B: 8位 -> 5位 */
        color = (r << 11) | (g << 5) | b;
        *output_pt++ = color & 0xff;
        *output_pt++ = (color >> 8) & 0xff;
			
		/* 第二个像素：YUV转RGB */
		r = R_FROMYV(Y1,V);
		g = G_FROMYUV(Y1,U,V);
		b = B_FROMYU(Y1,U);
		
        /* 将r,g,b颜色转换为RGB565的16位值 */
        r = r >> 3;
        g = g >> 2;
        b = b >> 3;
        color = (r << 11) | (g << 5) | b;
        *output_pt++ = color & 0xff;
        *output_pt++ = (color >> 8) & 0xff;
	}
	
	return 0;
} 




/**
 * @brief 将YUV422Packed格式转换为RGB32格式
 * 
 * @param input_ptr 输入YUV数据指针
 * @param output_ptr 输出RGB数据指针
 * @param image_width 图像宽度
 * @param image_height 图像高度
 * @return 0 - 成功
 */
static unsigned int
Pyuv422torgb32(unsigned char * input_ptr, unsigned char * output_ptr, unsigned int image_width, unsigned int image_height)
{
	unsigned int i, size;
	unsigned char Y, Y1, U, V;
	unsigned char *buff = input_ptr;
	unsigned int *output_pt = (unsigned int *)output_ptr;

    unsigned int r, g, b;
    unsigned int color;

	size = image_width * image_height /2;
	for (i = size; i > 0; i--) {
		/* 从YUV422格式中取出Y、U、V分量 */
		Y = buff[0] ;
		U = buff[1] ;
		Y1 = buff[2];
		V = buff[3];
		buff += 4;

        /* 第一个像素：YUV转RGB */
        r = R_FROMYV(Y,V);
		g = G_FROMYUV(Y,U,V);
		b = B_FROMYU(Y,U);
		/* 转换为RGB888格式 */
		color = (r << 16) | (g << 8) | b;
        *output_pt++ = color;
			
		/* 第二个像素：YUV转RGB */
		r = R_FROMYV(Y1,V);
		g = G_FROMYUV(Y1,U,V);
		b = B_FROMYU(Y1,U);
		color = (r << 16) | (g << 8) | b;
        *output_pt++ = color;
	}
	
	return 0;
} 

/**
 * @brief YUV转RGB转换函数
 * 
 * 根据输出格式调用相应的转换函数
 * 
 * @param ptVideoBufIn 输入视频缓冲区
 * @param ptVideoBufOut 输出视频缓冲区
 * @return 0 - 成功, -1 - 失败
 */
/* 参考luvcview */
static int Yuv2RgbConvert(PT_VideoBuf ptVideoBufIn, PT_VideoBuf ptVideoBufOut)
{
    PT_PixelDatas ptPixelDatasIn  = &ptVideoBufIn->tPixelDatas;
    PT_PixelDatas ptPixelDatasOut = &ptVideoBufOut->tPixelDatas;

    ptPixelDatasOut->iWidth  = ptPixelDatasIn->iWidth;
    ptPixelDatasOut->iHeight = ptPixelDatasIn->iHeight;
    
    if (ptVideoBufOut->iPixelFormat == V4L2_PIX_FMT_RGB565)
    {
        ptPixelDatasOut->iBpp = 16;
        ptPixelDatasOut->iLineBytes  = ptPixelDatasOut->iWidth * ptPixelDatasOut->iBpp / 8;
        ptPixelDatasOut->iTotalBytes = ptPixelDatasOut->iLineBytes * ptPixelDatasOut->iHeight;

        if (!ptPixelDatasOut->aucPixelDatas)
        {
            ptPixelDatasOut->aucPixelDatas = malloc(ptPixelDatasOut->iTotalBytes);
        }
        
        Pyuv422torgb565(ptPixelDatasIn->aucPixelDatas, ptPixelDatasOut->aucPixelDatas, ptPixelDatasOut->iWidth, ptPixelDatasOut->iHeight);
        return 0;
    }
    else if (ptVideoBufOut->iPixelFormat == V4L2_PIX_FMT_RGB32)
    {
        ptPixelDatasOut->iBpp = 32;
        ptPixelDatasOut->iLineBytes  = ptPixelDatasOut->iWidth * ptPixelDatasOut->iBpp / 8;
        ptPixelDatasOut->iTotalBytes = ptPixelDatasOut->iLineBytes * ptPixelDatasOut->iHeight;

        if (!ptPixelDatasOut->aucPixelDatas)
        {
            ptPixelDatasOut->aucPixelDatas = malloc(ptPixelDatasOut->iTotalBytes);
        }
        
        Pyuv422torgb32(ptPixelDatasIn->aucPixelDatas, ptPixelDatasOut->aucPixelDatas, ptPixelDatasOut->iWidth, ptPixelDatasOut->iHeight);
        return 0;
    }
    
    return -1;
}

/**
 * @brief 释放YUV转RGB转换缓冲区
 * 
 * @param ptVideoBufOut 输出视频缓冲区
 * @return 0 - 成功
 */
static int Yuv2RgbConvertExit(PT_VideoBuf ptVideoBufOut)
{
    if (ptVideoBufOut->tPixelDatas.aucPixelDatas)
    {
        free(ptVideoBufOut->tPixelDatas.aucPixelDatas);
        ptVideoBufOut->tPixelDatas.aucPixelDatas = NULL;
    }
    return 0;
}

/* 定义YUV转RGB转换器结构体 */
static T_VideoConvert g_tYuv2RgbConvert = {
    .name        = "yuv2rgb",
    .isSupport   = isSupportYuv2Rgb,
    .Convert     = Yuv2RgbConvert,
    .ConvertExit = Yuv2RgbConvertExit,
};

extern void initLut(void);

/* 注册YUV转RGB转换器 */
int Yuv2RgbInit(void)
{
    initLut();  /* 初始化颜色转换查找表 */
    return RegisterVideoConvert(&g_tYuv2RgbConvert);
}
