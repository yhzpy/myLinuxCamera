/**
 * @file merge.c
 * @brief 图像合并实现文件
 * 
 * 实现图像的合并功能，将小图像复制到大图像的指定位置
 */

#include <pic_operation.h>
#include <string.h>

/**********************************************************************
 * 函数名称: PicMerge
 * 功能描述: 将小图片合并到大图片中
 * 输入参数: iX,iY      - 小图片合并到大图片中的起始坐标, iX/iY确定小图片左上角的位置
 *           ptSmallPic - 含小图片像素数据的结构体
 *           ptBigPic   - 含大图片像素数据的结构体
 * 输出参数: 无
 * 返 回 值: 0 - 成功, 其他值 - 失败
 ***********************************************************************/
int PicMerge(int iX, int iY, PT_PixelDatas ptSmallPic, PT_PixelDatas ptBigPic)
{
	int i;
	unsigned char *pucSrc;
	unsigned char *pucDst;
	
	/* 检查小图片尺寸是否超过大图片，以及BPP是否一致 */
	if ((ptSmallPic->iWidth > ptBigPic->iWidth)  ||
		(ptSmallPic->iHeight > ptBigPic->iHeight) ||
		(ptSmallPic->iBpp != ptBigPic->iBpp))
	{
		return -1;
	}

	pucSrc = ptSmallPic->aucPixelDatas;
	/* 计算大图片中起始位置的地址 */
	pucDst = ptBigPic->aucPixelDatas + iY * ptBigPic->iLineBytes + iX * ptBigPic->iBpp / 8;
	
	/* 逐行复制小图片到大图片 */
	for (i = 0; i < ptSmallPic->iHeight; i++)
	{
		memcpy(pucDst, pucSrc, ptSmallPic->iLineBytes);
		pucSrc += ptSmallPic->iLineBytes;
		pucDst += ptBigPic->iLineBytes;
	}
	return 0;
}


/**********************************************************************
 * 函数名称: PicMergeRegion
 * 功能描述: 将大图片的某一块区域，合并到大图片的指定区域
 * 输入参数: iStartXofNewPic, iStartYofNewPic : 新图片的(iStartXofNewPic, iStartYofNewPic)坐标处开始被复制并合并
 *           iStartXofOldPic, iStartYofOldPic : 合并到新图片的(iStartXofOldPic, iStartYofOldPic)位置上去
 *           iWidth, iHeight                  : 合并区域的大小
 *           ptNewPic                         : 新图片
 *           ptOldPic                         : 老图片
 * 输出参数: 无
 * 返 回 值: 0 - 成功, 其他值 - 失败
 ***********************************************************************/
int PicMergeRegion(int iStartXofNewPic, int iStartYofNewPic, int iStartXofOldPic, int iStartYofOldPic, int iWidth, int iHeight, PT_PixelDatas ptNewPic, PT_PixelDatas ptOldPic)
{
	int i;
	unsigned char *pucSrc;
	unsigned char *pucDst;
    int iLineBytesCpy = iWidth * ptNewPic->iBpp / 8;

    /* 检查坐标是否超出范围 */
    if ((iStartXofNewPic < 0 || iStartXofNewPic >= ptNewPic->iWidth) || \
        (iStartYofNewPic < 0 || iStartYofNewPic >= ptNewPic->iHeight) || \
        (iStartXofOldPic < 0 || iStartXofOldPic >= ptOldPic->iWidth) || \
        (iStartYofOldPic < 0 || iStartYofOldPic >= ptOldPic->iHeight))
    {
        return -1;
    }
	
	/* 计算源和目标地址 */
	pucSrc = ptNewPic->aucPixelDatas + iStartYofNewPic * ptNewPic->iLineBytes + iStartXofNewPic * ptNewPic->iBpp / 8;
	pucDst = ptOldPic->aucPixelDatas + iStartYofOldPic * ptOldPic->iLineBytes + iStartXofOldPic * ptOldPic->iBpp / 8;
	
	/* 逐行复制区域数据 */
	for (i = 0; i < iHeight; i++)
	{
		memcpy(pucDst, pucSrc, iLineBytesCpy);
		pucSrc += ptNewPic->iLineBytes;
		pucDst += ptOldPic->iLineBytes;
	}
	return 0;
}
