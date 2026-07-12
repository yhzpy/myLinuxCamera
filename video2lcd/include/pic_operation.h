#ifndef PIC_OPERATION_H
#define PIC_OPERATION_H

#include <file.h>
/*
 * PixelDatas结构体 - 像素数据描述
 * 作用: 统一描述一块图像数据的基本信息, 包括尺寸、色深、行字节数等
 *       是系统中所有图像数据的通用表示, 被显示、渲染、采集等模块广泛使用
 */
typedef struct PixelDatas{
    int iWidth;
    int iHeight;
    int iBpp;
    int iLineBytes;
    int iTotalBytes;
    unsigned char *aucPixelDatas;
}T_PixelDatas, *PT_PixelDatas;
#endif // PIC_OPERATION_H