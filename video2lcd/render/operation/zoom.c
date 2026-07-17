/**
 * @file zoom.c
 * @brief 图像缩放实现文件
 * 
 * 实现图像的缩放功能，使用最近邻插值算法
 * 最近邻插值：选择距离目标像素最近的源像素作为目标像素的值
 * 优点：计算简单，速度快
 * 缺点：缩放后图像可能出现锯齿
 */

#include <config.h>
#include <pic_operation.h>
#include <stdlib.h>
#include <string.h>


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
int PicZoom(PT_PixelDatas ptOriginPic, PT_PixelDatas ptZoomPic)
{
    unsigned long dwDstWidth = ptZoomPic->iWidth;
    unsigned long* pdwSrcXTable;   /* 源图像X坐标查找表 */
	unsigned long x;
	unsigned long y;
	unsigned long dwSrcY;          /* 源图像Y坐标 */
	unsigned char *pucDest;        /* 目标图像数据指针 */
	unsigned char *pucSrc;         /* 源图像数据指针 */
	unsigned long dwPixelBytes = ptOriginPic->iBpp/8;  /* 每像素字节数 */

    DBG_PRINTF("src:\n");
    DBG_PRINTF("%d x %d, %d bpp, data: 0x%x\n", ptOriginPic->iWidth, ptOriginPic->iHeight, ptOriginPic->iBpp, (unsigned int)ptOriginPic->aucPixelDatas);

    DBG_PRINTF("dest:\n");
    DBG_PRINTF("%d x %d, %d bpp, data: 0x%x\n", ptZoomPic->iWidth, ptZoomPic->iHeight, ptZoomPic->iBpp, (unsigned int)ptZoomPic->aucPixelDatas);

	/* 检查输入输出图像的BPP是否一致 */
	if (ptOriginPic->iBpp != ptZoomPic->iBpp)
	{
		return -1;
	}

    /* 分配源图像X坐标查找表 */
    pdwSrcXTable = malloc(sizeof(unsigned long) * dwDstWidth);
    if (NULL == pdwSrcXTable)
    {
        DBG_PRINTF("malloc error!\n");
        return -1;
    }

    /* 构建X坐标映射表：目标X坐标 -> 源X坐标 */
    for (x = 0; x < dwDstWidth; x++)
    {
        pdwSrcXTable[x]=(x*ptOriginPic->iWidth/ptZoomPic->iWidth);
    }

    /* 逐行进行缩放 */
    for (y = 0; y < ptZoomPic->iHeight; y++)
    {			
        /* 计算当前行对应的源图像行 */
        dwSrcY = (y * ptOriginPic->iHeight / ptZoomPic->iHeight);

		pucDest = ptZoomPic->aucPixelDatas + y*ptZoomPic->iLineBytes;
		pucSrc  = ptOriginPic->aucPixelDatas + dwSrcY*ptOriginPic->iLineBytes;
		
        /* 对当前行的每个像素进行缩放 */
        for (x = 0; x <dwDstWidth; x++)
        {
            /* 源图像坐标: pdwSrcXTable[x], dwSrcY
             * 目标图像坐标: x, y
			 */
			 memcpy(pucDest+x*dwPixelBytes, pucSrc+pdwSrcXTable[x]*dwPixelBytes, dwPixelBytes);
        }
    }

    free(pdwSrcXTable);
	return 0;
}
