/**
 * @file color.c
 * @brief 颜色空间转换查找表(LUT)实现文件
 *
 * 实现YUV到RGB的颜色转换查找表, 供yuv2rgb.c调用
 *
 * 转换公式（与yuv2rgb.c注释一致）：
 * R = Y + 1.4075 * (V - 128)
 * G = Y - 0.3455 * (U - 128) - 0.7169 * (V - 128)
 * B = Y + 1.7790 * (U - 128)
 */

#include "color.h"

/* 查找表: 以U/V分量(0~255)为索引, 存放对应的转换系数 */
static int g_lutRv[256];   /* R: 1.4075 * (v - 128)  */
static int g_lutGu[256];   /* G: -0.3455 * (u - 128) */
static int g_lutGv[256];   /* G: -0.7169 * (v - 128) */
static int g_lutBu[256];   /* B: 1.7790 * (u - 128)  */

/* RGB转Y（亮度），BT.601系数: Y = (77R + 150G + 29B) >> 8 */
unsigned char RGB24_TO_Y(unsigned char r, unsigned char g, unsigned char b)
{
    return YfromRGB(r, g, b);
}

/* 由R和Y计算V分量: V = 128 + 0.713 * (R - Y) */
unsigned char YR_TO_V(unsigned char r, unsigned char y)
{
    return CLIP((((int)r - (int)y) * 713 / 1000) + 128);
}

/* 由B和Y计算U分量: U = 128 + 0.564 * (B - Y) */
unsigned char YB_TO_U(unsigned char b, unsigned char y)
{
    return CLIP((((int)b - (int)y) * 564 / 1000) + 128);
}

/* R = Y + 1.4075 * (V - 128) */
unsigned char R_FROMYV(unsigned char y, unsigned char v)
{
    return CLIP((y) + g_lutRv[(v)]);
}

/* G = Y - 0.3455 * (U - 128) - 0.7169 * (V - 128) */
unsigned char G_FROMYUV(unsigned char y, unsigned char u, unsigned char v)
{
    return CLIP((y) + g_lutGu[(u)] + g_lutGv[(v)]);
}

/* B = Y + 1.7790 * (U - 128) */
unsigned char B_FROMYU(unsigned char y, unsigned char u)
{
    return CLIP((y) + g_lutBu[(u)]);
}

/* 初始化颜色转换查找表 */
void initLut(void)
{
    int i;
    for (i = 0; i < 256; i++)
    {
        g_lutRv[i] = (int)(1.4075 * (i - 128));
        g_lutGu[i] = (int)(-0.3455 * (i - 128));
        g_lutGv[i] = (int)(-0.7169 * (i - 128));
        g_lutBu[i] = (int)(1.7790 * (i - 128));
    }
}

/* 释放查找表(本项目查找表为静态数组, 无需释放) */
void freeLut(void)
{
}
