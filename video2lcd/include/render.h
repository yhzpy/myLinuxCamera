/**
 * @file render.h
 * @brief 图像渲染操作头文件
 * 
 * 定义图像缩放和合并等渲染操作接口
 * 用于图像处理和显示
 */

#ifndef _RENDER_H
#define _RENDER_H

#include <pic_operation.h>
#include <disp_manager.h>

/**********************************************************************
 * 函数名称: PicZoom
 * 功能描述: 按目标尺寸缩放图片（使用最近邻插值算法）
 *           注意：调用此函数前需分配好目标图片的内存，使用完后需要free释放
 *           "最近邻插值"原理参考网上"lantianyu520"所写"图像缩放算法"
 * 输入参数: ptOriginPic - 含原始图片像素数据的结构体
 *           ptBigPic    - 含目标尺寸图片像素数据的结构体
 * 输出参数: 无
 * 返 回 值: 0 - 成功, 其他值 - 失败
 ***********************************************************************/
int PicZoom(PT_PixelDatas ptOriginPic, PT_PixelDatas ptZoomPic);

/**********************************************************************
 * 函数名称: PicMerge
 * 功能描述: 将小图片合并到大图片中
 * 输入参数: iX,iY      - 小图片合并到大图片中的起始坐标, iX/iY确定小图片左上角的位置
 *           ptSmallPic - 含小图片像素数据的结构体
 *           ptBigPic   - 含大图片像素数据的结构体
 * 输出参数: 无
 * 返 回 值: 0 - 成功, 其他值 - 失败
 ***********************************************************************/
int PicMerge(int iX, int iY, PT_PixelDatas ptSmallPic, PT_PixelDatas ptBigPic);


#endif /* _RENDER_H */
