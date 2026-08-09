/**
 * @file disp_manager.c
 * @brief 显示设备管理实现文件
 * 
 * 实现显示设备的注册、选择、VideoMem管理等功能
 * 支持多种显示设备的动态注册和切换
 */

#include <config.h>
#include <disp_manager.h>
#include <string.h>

/* 全局变量：显示操作接口链表头 */
static PT_DispOpr g_ptDispOprHead;
/* 全局变量：默认使用的显示操作接口 */
static PT_DispOpr g_ptDefaultDispOpr;
/* 全局变量：VideoMem链表头 */
static PT_VideoMem g_ptVideoMemHead;

/**********************************************************************
 * 函数名称: RegisterDispOpr
 * 功能描述: 注册"显示模块"，支持多种显示设备的操作接口注册
 * 输入参数: ptDispOpr - 一个结构体指针，含显示设备的操作接口
 * 输出参数: 无
 * 返 回 值: 0 - 成功, 其他值 - 失败
 ***********************************************************************/
int RegisterDispOpr(PT_DispOpr ptDispOpr)
{
	PT_DispOpr ptTmp;

	if (!g_ptDispOprHead)
	{
		g_ptDispOprHead   = ptDispOpr;
		ptDispOpr->ptNext = NULL;
	}
	else
	{
		ptTmp = g_ptDispOprHead;
		while (ptTmp->ptNext)
		{
			ptTmp = ptTmp->ptNext;
		}
		ptTmp->ptNext	  = ptDispOpr;
		ptDispOpr->ptNext = NULL;
	}

	return 0;
}


/**********************************************************************
 * 函数名称: ShowDispOpr
 * 功能描述: 显示所有支持的"显示模块"列表
 * 输入参数: 无
 * 输出参数: 无
 * 返 回 值: 无
 ***********************************************************************/
void ShowDispOpr(void)
{
	int i = 0;
	PT_DispOpr ptTmp = g_ptDispOprHead;

	while (ptTmp)
	{
		printf("%02d %s\n", i++, ptTmp->name);
		ptTmp = ptTmp->ptNext;
	}
}

/**********************************************************************
 * 函数名称: GetDispOpr
 * 功能描述: 根据名称获取指定的"显示模块"
 * 输入参数: pcName - 显示模块名称
 * 输出参数: 无
 * 返 回 值: NULL   - 失败,没有指定名称的模块, 
 *           非NULL - 显示模块的PT_DispOpr结构体指针
 ***********************************************************************/
PT_DispOpr GetDispOpr(char *pcName)
{
	PT_DispOpr ptTmp = g_ptDispOprHead;
	
	while (ptTmp)
	{
		if (strcmp(ptTmp->name, pcName) == 0)
		{
			return ptTmp;
		}
		ptTmp = ptTmp->ptNext;
	}
	return NULL;
}

/**********************************************************************
 * 函数名称: SelectAndInitDefaultDispDev
 * 功能描述: 根据名称获取指定的"显示模块"，执行初始化操作，清空屏幕
 * 输入参数: name - 显示设备名称
 * 输出参数: 无
 * 返 回 值: 无
 ***********************************************************************/
void SelectAndInitDefaultDispDev(char *name)
{
    g_ptDefaultDispOpr = GetDispOpr(name);
    if(g_ptDefaultDispOpr)
    {
        g_ptDefaultDispOpr->DeviceInit();
        g_ptDefaultDispOpr->CleanScreen(0);
    }
}

PT_DispOpr GetDefaultDispDev(void)
{
	return g_ptDefaultDispOpr;
}

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
int GetDispResolution(int *piXres, int *piYres, int *piBpp)
{
	if (g_ptDefaultDispOpr)
	{
		*piXres = g_ptDefaultDispOpr->iXres;
		*piYres = g_ptDefaultDispOpr->iYres;
		*piBpp  = g_ptDefaultDispOpr->iBpp;
		return 0;
	}
	else
	{
		return -1;
	}
}

/**
 * @brief 获取用于显示的帧缓冲区
 * 
 * 根据LCD的色深设置对应的像素格式，并填充帧缓冲区信息
 * 
 * @param ptFrameBuf 帧缓冲区结构体指针
 * @return 0 - 成功
 */
int GetVideoBufForDisplay(PT_VideoBuf ptFrameBuf)
{
    /* 根据BPP设置对应的像素格式 */
    ptFrameBuf->iPixelFormat = (g_ptDefaultDispOpr->iBpp == 16) ? V4L2_PIX_FMT_RGB565 : \
                                   (g_ptDefaultDispOpr->iBpp == 32) ?  V4L2_PIX_FMT_RGB32 : \
                                           0;
    /* 填充帧缓冲区的尺寸信息 */
    ptFrameBuf->tPixelDatas.iWidth  = g_ptDefaultDispOpr->iXres;
    ptFrameBuf->tPixelDatas.iHeight = g_ptDefaultDispOpr->iYres;
    ptFrameBuf->tPixelDatas.iBpp    = g_ptDefaultDispOpr->iBpp;
    ptFrameBuf->tPixelDatas.iLineBytes    = g_ptDefaultDispOpr->iLineWidth;
    ptFrameBuf->tPixelDatas.iTotalBytes   = ptFrameBuf->tPixelDatas.iLineBytes * ptFrameBuf->tPixelDatas.iHeight;
    /* 直接指向显示设备的framebuffer显存 */
    ptFrameBuf->tPixelDatas.aucPixelDatas = g_ptDefaultDispOpr->pucDispMem;
    return 0;    
}

/**
 * @brief 将像素数据刷新到显示设备
 * 
 * 调用显示设备的ShowPage方法将数据输出到LCD
 * 
 * @param ptPixelDatas 像素数据指针
 */
void FlushPixelDatasToDev(PT_PixelDatas ptPixelDatas)/*调用显示设备的ShowPage方法将数据输出到LCD*/
{
    g_ptDefaultDispOpr->ShowPage(ptPixelDatas);
}

/*关于VideoMem的部分 main并未用到 未来配合lvgl开发*/
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
int AllocVideoMem(int iNum)
{
    int i;
    int iXres = 0;
    int iYres = 0;
    int iBpp = 0;
    int iVMSize = 0; /* VideoMem大小 */
    int iLineBytes = 0;

    PT_VideoMem ptNew;
    /* 步骤1：确定VideoMem的大小（与LCD分辨率相同） */
    GetDispResolution(&iXres, &iYres, &iBpp);
    iVMSize = iXres * iYres * iBpp /8;
    iLineBytes = iXres * iBpp / 8;

    
	/* 步骤2：首先把设备本身的framebuffer封装成VideoMem
	 * 分配一个T_VideoMem结构体，注意没有分配额外的tPixelDatas.aucPixelDatas
	 * 而是让tPixelDatas.aucPixelDatas指向显示设备的framebuffer
	 */
    ptNew = malloc(sizeof(T_VideoMem));
    if(ptNew == NULL)
    {
        return -1;
    }

    
	/* 指向framebuffer显存地址 */
	ptNew->tPixelDatas.aucPixelDatas = g_ptDefaultDispOpr->pucDispMem;
	
	ptNew->iID = 0;
	ptNew->bDevFrameBuffer = 1;        /* 表示这个VideoMem是设备本身的framebuffer, 后续分配的都是普通的VideoMem */
	ptNew->eVideoMemState  = VMS_FREE;
	ptNew->ePicState	   = PS_BLANK;
	ptNew->tPixelDatas.iWidth  = iXres;
	ptNew->tPixelDatas.iHeight = iYres;
	ptNew->tPixelDatas.iBpp    = iBpp;
	ptNew->tPixelDatas.iLineBytes  = iLineBytes;
	ptNew->tPixelDatas.iTotalBytes = iVMSize;
    
    if(iNum != 0)
    {
        ptNew->eVideoMemState = VMS_USED_FOR_CUR;
    }

    ptNew->ptNext = g_ptVideoMemHead; /*没赋值链表头 他自动指向null 这是特性*/
    g_ptVideoMemHead = ptNew;

    for(i = 0; i < iNum;i++)
    {
        ptNew = malloc(sizeof(T_VideoMem) + iVMSize);
        if(ptNew == NULL)
        {
            return -1;
        }
        ptNew->tPixelDatas.aucPixelDatas = (unsigned char *)(ptNew + 1); /*紧跟在结构体后面*/
        ptNew->iID = 0;
        ptNew->bDevFrameBuffer = 0;    /* 表示这个VideoMem是普通的VideoMem */
        ptNew->eVideoMemState  = VMS_FREE;
        ptNew->ePicState	   = PS_BLANK;
        ptNew->tPixelDatas.iWidth  = iXres;
        ptNew->tPixelDatas.iHeight = iYres;
        ptNew->tPixelDatas.iBpp    = iBpp;
        ptNew->tPixelDatas.iLineBytes  = iLineBytes;
        ptNew->tPixelDatas.iTotalBytes = iVMSize;

        ptNew->ptNext = g_ptVideoMemHead;
        g_ptVideoMemHead = ptNew;
    }

    return 0;
}

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
PT_VideoMem GetVideoMem(int iID, int bCur)/*ID的策略还需要优化 项目没有多界面*/
{
    PT_VideoMem ptTmp = g_ptVideoMemHead;
    while(ptTmp)
    {
        if((ptTmp -> eVideoMemState == VMS_FREE) && (ptTmp->iID == iID))
        {
            ptTmp->eVideoMemState = bCur ? VMS_USED_FOR_CUR : VMS_USED_FOR_PREPARE;
			return ptTmp;
        }
        ptTmp = ptTmp->ptNext;
    }
	/* 策略2: 如果前面不成功, 取一个"空闲的并且没有生成图像(ptVideoMem->ePicState = PS_BLANK)"的VideoMem */
	ptTmp = g_ptVideoMemHead;
	while (ptTmp)
	{
		if ((ptTmp->eVideoMemState == VMS_FREE) && (ptTmp->ePicState == PS_BLANK))
		{
			ptTmp->iID = iID;
			ptTmp->eVideoMemState = bCur ? VMS_USED_FOR_CUR : VMS_USED_FOR_PREPARE;
			return ptTmp;
		}
		ptTmp = ptTmp->ptNext;
	}	
	
	/* 策略3: 如果前面不成功: 取随便一个空闲的VideoMem */
	ptTmp = g_ptVideoMemHead;
	while (ptTmp)
	{
		if (ptTmp->eVideoMemState == VMS_FREE)
		{
			ptTmp->iID = iID;
			ptTmp->ePicState = PS_BLANK;
			ptTmp->eVideoMemState = bCur ? VMS_USED_FOR_CUR : VMS_USED_FOR_PREPARE;
			return ptTmp;
		}
		ptTmp = ptTmp->ptNext;
	}

    /* 策略4: 如果没有空闲的VideoMem但bCur为1, 则取随便一个VideoMem(不管它是否空闲) */
    if (bCur)
    {
    	ptTmp = g_ptVideoMemHead;
    	ptTmp->iID = iID;
    	ptTmp->ePicState = PS_BLANK;
    	ptTmp->eVideoMemState = bCur ? VMS_USED_FOR_CUR : VMS_USED_FOR_PREPARE;
    	return ptTmp;
    }
    
	return NULL;
}
/**********************************************************************
 * 函数名称: PutVideoMem
 * 功能描述: 释放使用GetVideoMem获取的VideoMem
 * 输入参数: ptVideoMem - 使用完毕的VideoMem
 * 输出参数: 无
 * 返 回 值: 无
 ***********************************************************************/
void PutVideoMem(PT_VideoMem ptVideoMem)
{
	ptVideoMem->eVideoMemState = VMS_FREE;  /* 设置VideoMem状态为空闲 */
    if (ptVideoMem->iID == -1)
    {
        ptVideoMem->ePicState = PS_BLANK;  /* 表示此页已经没有有效图像 */
    }
}

/**********************************************************************
 * 函数名称: GetDevVideoMem
 * 功能描述: 获取显示设备的显存VideoMem，在该显存上操作可直接在LCD上显示
 * 输入参数: 无
 * 输出参数: 无
 * 返 回 值: 显存对应的VideoMem结构体指针
 ***********************************************************************/
PT_VideoMem GetDevVideoMem(void)
{
	PT_VideoMem ptTmp = g_ptVideoMemHead;
	
	while (ptTmp)
	{
		if (ptTmp->bDevFrameBuffer)
		{
			return ptTmp;
		}
		ptTmp = ptTmp->ptNext;
	}
	return NULL;
}


/**********************************************************************
 * 函数名称: ClearVideoMem
 * 功能描述: 将VideoMem的内存全部设置为某一种颜色
 * 输入参数: ptVideoMem - VideoMem结构体指针，含要清空的内存
 *           dwColor    - 清空为什么颜色
 * 输出参数: 无
 * 返 回 值: 无
 ***********************************************************************/
void ClearVideoMem(PT_VideoMem ptVideoMem, unsigned int dwColor)
{
	unsigned char *pucVM;
	unsigned short *pwVM16bpp;
	unsigned int *pdwVM32bpp;
	unsigned short wColor16bpp; /* 565格式 */
	int iRed;
	int iGreen;
	int iBlue;
	int i = 0;

	pucVM	   = ptVideoMem->tPixelDatas.aucPixelDatas;
	pwVM16bpp  = (unsigned short *)pucVM;
	pdwVM32bpp = (unsigned int *)pucVM;

	switch (ptVideoMem->tPixelDatas.iBpp)
	{
		case 8:
		{
			memset(pucVM, dwColor, ptVideoMem->tPixelDatas.iTotalBytes);
			break;
		}
		case 16:
		{
			/* 先把32位的dwColor转换为16位的wColor16bpp */
			iRed   = (dwColor >> (16+3)) & 0x1f;
			iGreen = (dwColor >> (8+2)) & 0x3f;
			iBlue  = (dwColor >> 3) & 0x1f;
			wColor16bpp = (iRed << 11) | (iGreen << 5) | iBlue;
			while (i < ptVideoMem->tPixelDatas.iTotalBytes)
			{
				*pwVM16bpp	= wColor16bpp;
				pwVM16bpp++;
				i += 2;
			}
			break;
		}
		case 32:
		{
			while (i < ptVideoMem->tPixelDatas.iTotalBytes)
			{
				*pdwVM32bpp = dwColor;
				pdwVM32bpp++;
				i += 4;
			}
			break;
		}
		default :
		{
			DBG_PRINTF("can't support %d bpp\n", ptVideoMem->tPixelDatas.iBpp);
			return;
		}
	}

}


/**********************************************************************
 * 函数名称: ClearVideoMemRegion
 * 功能描述: 将VideoMem的内存中指定区域全部设置为某一种颜色
 * 输入参数: ptVideoMem - VideoMem结构体指针，含要清空的内存
 *           ptLayout   - 布局结构，指定左上角和右下角的坐标
 *           dwColor    - 清空为什么颜色
 * 输出参数: 无
 * 返 回 值: 无
 ***********************************************************************/
void ClearVideoMemRegion(PT_VideoMem ptVideoMem, PT_Layout ptLayout, unsigned int dwColor)
{
	unsigned char *pucVM;
	unsigned short *pwVM16bpp;
	unsigned int *pdwVM32bpp;
	unsigned short wColor16bpp; /* 565格式 */
	int iRed;
	int iGreen;
	int iBlue;
	int iX;
	int iY;
    int iLineBytesClear;
    int i;

	pucVM	   = ptVideoMem->tPixelDatas.aucPixelDatas + ptLayout->iTopLeftY * ptVideoMem->tPixelDatas.iLineBytes + ptLayout->iTopLeftX * ptVideoMem->tPixelDatas.iBpp / 8;
	pwVM16bpp  = (unsigned short *)pucVM;
	pdwVM32bpp = (unsigned int *)pucVM;

    iLineBytesClear = (ptLayout->iBotRightX - ptLayout->iTopLeftX + 1) * ptVideoMem->tPixelDatas.iBpp / 8;

	switch (ptVideoMem->tPixelDatas.iBpp)
	{
		case 8:
		{
            for (iY = ptLayout->iTopLeftY; iY <= ptLayout->iBotRightY; iY++)
            {
    			memset(pucVM, dwColor, iLineBytesClear);
                pucVM += ptVideoMem->tPixelDatas.iLineBytes;
            }
			break;
		}
		case 16:
		{
			/* 先把32位的dwColor转换为16位的wColor16bpp */
			iRed   = (dwColor >> (16+3)) & 0x1f;
			iGreen = (dwColor >> (8+2)) & 0x3f;
			iBlue  = (dwColor >> 3) & 0x1f;
			wColor16bpp = (iRed << 11) | (iGreen << 5) | iBlue;
            for (iY = ptLayout->iTopLeftY; iY <= ptLayout->iBotRightY; iY++)
            {
                i = 0;
                for (iX = ptLayout->iTopLeftX; iX <= ptLayout->iBotRightX; iX++)
    			{
    				pwVM16bpp[i++]	= wColor16bpp;
    			}
                pwVM16bpp = (unsigned short *)((unsigned int)pwVM16bpp + ptVideoMem->tPixelDatas.iLineBytes);
            }
			break;
		}
		case 32:
		{
            for (iY = ptLayout->iTopLeftY; iY <= ptLayout->iBotRightY; iY++)
            {
                i = 0;
                for (iX = ptLayout->iTopLeftX; iX <= ptLayout->iBotRightX; iX++)
    			{
    				pdwVM32bpp[i++]	= dwColor;
    			}
                pdwVM32bpp = (unsigned int *)((unsigned int)pdwVM32bpp + ptVideoMem->tPixelDatas.iLineBytes);
            }
			break;
		}
		default :
		{
			DBG_PRINTF("can't support %d bpp\n", ptVideoMem->tPixelDatas.iBpp);
			return;
		}
	}

}

/**********************************************************************
 * 函数名称: DisplayInit
 * 功能描述: 注册所有显示设备
 * 输入参数: 无
 * 输出参数: 无
 * 返 回 值: 0 - 成功, 其他值 - 失败
 ***********************************************************************/
int DisplayInit(void)
{
	int iError;
	
	iError = FBInit();

	return iError;
}
