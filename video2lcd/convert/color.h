/****************************************************************************
#       GspcaGui:  Gspca/Spca5xx Grabber                                  #
#       Copyright (C) 2004 2005 2006 Michel Xhaard                         #
#                                                                           #
# This program is free software; you can redistribute it and/or modify      #
# it under the terms of the GNU General Public License as published by      #
# the Free Software Foundation; either version 2 of the License, or         #
# (at your option) any later version.                                       #
#                                                                           #
# This program is distributed in the hope that it will be useful,           #
# but WITHOUT ANY WARRANTY; without even the implied warranty of            #
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the             #
# GNU General Public License for more details.                              #
#                                                                           #
# You should have received a copy of the GNU General Public License         #
# along with this program; if not, write to the Free Software               #
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA #
#                                                                           #
****************************************************************************/ 

/**
 * @file color.h
 * @brief 颜色空间转换头文件
 * 
 * 定义颜色结构体和YUV/RGB转换相关的宏和函数
 */

/* RGB16结构体：RGB565格式 */
typedef struct Myrgb16 {
    unsigned short blue:5;   /* 蓝色：5位 */
    unsigned short green:6;  /* 绿色：6位 */
    unsigned short red:5;    /* 红色：5位 */
} Myrgb16;

/* RGB24结构体：RGB888格式 */
typedef struct Myrgb24 {
    unsigned char blue;      /* 蓝色：8位 */
    unsigned char green;     /* 绿色：8位 */
    unsigned char red;       /* 红色：8位 */
} Myrgb24;

/* RGB32结构体：带Alpha通道 */
typedef struct Myrgb32 {
    unsigned char blue;      /* 蓝色：8位 */
    unsigned char green;     /* 绿色：8位 */
    unsigned char red;       /* 红色：8位 */
    unsigned char alpha;     /* Alpha通道：8位 */
} Myrgb32;

/* YUV422结构体：每4字节表示2个像素 */
typedef struct MyYUV422 {
    unsigned char y0;        /* 第一个像素的亮度 */
    unsigned char u;         /* U色度分量（两个像素共享） */
    unsigned char y1;        /* 第二个像素的亮度 */
    unsigned char v;         /* V色度分量（两个像素共享） */
} MyYUV422;

/* YUV444结构体：每个像素独立存储 */
typedef struct MyYUV444 {
    unsigned char y;         /* 亮度 */
    unsigned char u;         /* U色度分量 */
    unsigned char v;         /* V色度分量 */
} MyYUV444;

/* 颜色值裁剪宏：确保颜色值在0-255范围内 */
#define CLIP(color) (unsigned char)(((color)>0xFF)?0xff:(((color)<0)?0:(color)))

/* 颜色转换函数声明 */
unsigned char RGB24_TO_Y(unsigned char r, unsigned char g, unsigned char b);  /* RGB转Y（亮度） */
unsigned char YR_TO_V(unsigned char r, unsigned char y);                       /* 计算V分量 */
unsigned char YB_TO_U(unsigned char b, unsigned char y);                       /* 计算U分量 */
unsigned char R_FROMYV(unsigned char y, unsigned char v);                      /* 从YV计算R */
unsigned char G_FROMYUV(unsigned char y, unsigned char u, unsigned char v);    /* 从YUV计算G */
unsigned char B_FROMYU(unsigned char y, unsigned char u);                      /* 从YU计算B */

/* YUV/RGB转换公式宏定义 */
#define YfromRGB(r,g,b) CLIP((77*(r)+150*(g)+29*(b))>>8)
#define UfromRGB(r,g,b) CLIP(((128*(b)-85*(g)-43*(r))>>8 )+128)
#define VfromRGB(r,g,b) CLIP(((128*(r)-107*(g)-21*(b))>>8) +128)

/* RGB16打包/解包宏 */
#define PACKRGB16(r,g,b) (__u16) ((((b) & 0xF8) << 8 ) | (((g) & 0xFC) << 3 ) | (((r) & 0xF8) >> 3 ))
#define UNPACK16(pixel,r,g,b) r=((pixel)&0xf800) >> 8; g=((pixel)&0x07e0) >> 3; b=(((pixel)&0x001f) << 3)

/* 查找表初始化和释放函数 */
void initLut(void);   /* 初始化颜色转换查找表 */
void freeLut(void);   /* 释放颜色转换查找表 */
