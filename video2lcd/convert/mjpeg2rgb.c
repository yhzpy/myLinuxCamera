/**
 * @file mjpeg2rgb.c
 * @brief MJPEG转RGB格式转换器实现文件
 * 
 * 实现MJPEG（Motion JPEG）格式到RGB格式的转换
 * MJPEG格式：每一帧数据都是一个完整的JPEG文件
 * 使用libjpeg库进行JPEG解码
 */

#include <convert_manager.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#include <jpeglib.h>

/**
 * @struct MyErrorMgr
 * @brief 自定义JPEG错误管理器
 * 
 * 用于处理libjpeg库的错误
 * 默认的错误处理会导致程序退出，这里使用setjmp/longjmp实现错误恢复
 */
typedef struct MyErrorMgr
{
	struct jpeg_error_mgr pub;      /* 标准错误管理器 */
	jmp_buf setjmp_buffer;          /* 错误恢复点 */
}T_MyErrorMgr, *PT_MyErrorMgr;

extern void jpeg_mem_src_tj(j_decompress_ptr, unsigned char *, unsigned long);

/**
 * @brief 判断是否支持MJPEG转RGB的格式转换
 * 
 * @param iPixelFormatIn 输入像素格式
 * @param iPixelFormatOut 输出像素格式
 * @return 1 - 支持, 0 - 不支持
 */
static int isSupportMjpeg2Rgb(int iPixelFormatIn, int iPixelFormatOut)
{
    if (iPixelFormatIn != V4L2_PIX_FMT_MJPEG)
        return 0;
    if ((iPixelFormatOut != V4L2_PIX_FMT_RGB565) && (iPixelFormatOut != V4L2_PIX_FMT_RGB32))
    {
        return 0;
    }
    return 1;
}


/**********************************************************************
 * 函数名称: MyErrorExit
 * 功能描述: 自定义的libjpeg错误处理函数
 *           默认的错误处理会导致程序退出，这里使用setjmp/longjmp实现错误恢复
 *           参考libjpeg库的bmp.c示例代码
 * 输入参数: ptCInfo - libjpeg错误处理结构的通用结构体
 * 输出参数: 无
 * 返 回 值: 无
 ***********************************************************************/
static void MyErrorExit(j_common_ptr ptCInfo)
{
    static char errStr[JMSG_LENGTH_MAX];
    
	PT_MyErrorMgr ptMyErr = (PT_MyErrorMgr)ptCInfo->err;

    /* 创建错误消息 */
    (*ptCInfo->err->format_message) (ptCInfo, errStr);
    DBG_PRINTF("%s\n", errStr);

	longjmp(ptMyErr->setjmp_buffer, 1);
}

/**********************************************************************
 * 函数名称: CovertOneLine
 * 功能描述: 将已经从JPG文件取出的一行数据，转换为适合显示设备使用的格式
 * 输入参数: iWidth      - 宽度，有多少个像素
 *           iSrcBpp     - 已经从JPG文件取出的一行数据的格式，一个像素用多少位来表示
 *           iDstBpp     - 显示设备的一个像素用多少位来表示
 *           pudSrcDatas - 已经从JPG文件取出的一行数据的存储位置
 *           pudDstDatas - 转换后的数据存储位置
 * 输出参数: 无
 * 返 回 值: 0 - 成功, 其他值 - 失败
 ***********************************************************************/
static int CovertOneLine(int iWidth, int iSrcBpp, int iDstBpp, unsigned char *pudSrcDatas, unsigned char *pudDstDatas)
{
	unsigned int dwRed;
	unsigned int dwGreen;
	unsigned int dwBlue;
	unsigned int dwColor;

	unsigned short *pwDstDatas16bpp = (unsigned short *)pudDstDatas;
	unsigned int   *pwDstDatas32bpp = (unsigned int *)pudDstDatas;

	int i;
	int pos = 0;

	if (iSrcBpp != 24)
	{
		return -1;
	}

	if (iDstBpp == 24)
	{
		memcpy(pudDstDatas, pudSrcDatas, iWidth*3);
	}
	else
	{
		for (i = 0; i < iWidth; i++)
		{
			/* 从24位RGB数据中取出R、G、B分量 */
			dwRed   = pudSrcDatas[pos++];
			dwGreen = pudSrcDatas[pos++];
			dwBlue  = pudSrcDatas[pos++];
			if (iDstBpp == 32)
			{
				/* 转换为32位RGB888格式 */
				dwColor = (dwRed << 16) | (dwGreen << 8) | dwBlue;
				*pwDstDatas32bpp = dwColor;
				pwDstDatas32bpp++;
			}
			else if (iDstBpp == 16)
			{
				/* 转换为16位RGB565格式 */
				dwRed   = dwRed >> 3;   /* R: 8位 -> 5位 */
				dwGreen = dwGreen >> 2; /* G: 8位 -> 6位 */
				dwBlue  = dwBlue >> 3;  /* B: 8位 -> 5位 */
				dwColor = (dwRed << 11) | (dwGreen << 5) | (dwBlue);
				*pwDstDatas16bpp = dwColor;
				pwDstDatas16bpp++;
			}
		}
	}
	return 0;
}


/**********************************************************************
 * 函数名称: Mjpeg2RgbConvert
 * 功能描述: 将MJPEG格式的图像数据取出，转换为适合显示设备使用的格式
 *           使用libjpeg库进行JPEG解码
 * 输入参数: ptVideoBufIn  - 含MJPEG数据的视频缓冲区
 *           ptVideoBufOut - 转换后的输出缓冲区
 * 输出参数: 无
 * 返 回 值: 0 - 成功, 其他值 - 失败
 ***********************************************************************/
static int Mjpeg2RgbConvert(PT_VideoBuf ptVideoBufIn, PT_VideoBuf ptVideoBufOut)
{
	struct jpeg_decompress_struct tDInfo;  /* JPEG解压缩结构体 */
    int iRet;
    int iRowStride;                        /* 一行数据的字节数 */
    unsigned char *aucLineBuffer = NULL;   /* 行缓冲区 */
    unsigned char *pucDest;                /* 目标数据指针 */
	T_MyErrorMgr tJerr;                    /* 自定义错误管理器 */
    PT_PixelDatas ptPixelDatas = &ptVideoBufOut->tPixelDatas;

	/* 设置错误处理 */
	tDInfo.err               = jpeg_std_error(&tJerr.pub);
	tJerr.pub.error_exit     = MyErrorExit;

	/* 设置错误恢复点 */
	if(setjmp(tJerr.setjmp_buffer))
	{
		/* 如果在处理过程中出现错误，显示JPEG错误信息 */
        jpeg_destroy_decompress(&tDInfo);
        if (aucLineBuffer)
        {
            free(aucLineBuffer);
        }
        if (ptPixelDatas->aucPixelDatas)
        {
            free(ptPixelDatas->aucPixelDatas);
        }
		return -1;
	}

	/* 创建JPEG解压缩对象 */
	jpeg_create_decompress(&tDInfo);

	/* 设置数据源为内存中的JPEG数据 */
    jpeg_mem_src_tj (&tDInfo, ptVideoBufIn->tPixelDatas.aucPixelDatas, ptVideoBufIn->tPixelDatas.iTotalBytes);
    

    /* 读取JPEG头部信息 */
    iRet = jpeg_read_header(&tDInfo, TRUE);

	/* 设置缩放参数，不进行缩放 */
    tDInfo.scale_num = tDInfo.scale_denom = 1;
    
	/* 开始解压缩 */
	jpeg_start_decompress(&tDInfo);
    
	/* 计算一行数据的字节数 */
	iRowStride = tDInfo.output_width * tDInfo.output_components;
	aucLineBuffer = malloc(iRowStride);

    if (NULL == aucLineBuffer)
    {
        return -1;
    }

	/* 填充输出像素数据结构 */
	ptPixelDatas->iWidth  = tDInfo.output_width;
	ptPixelDatas->iHeight = tDInfo.output_height;
	ptPixelDatas->iLineBytes    = ptPixelDatas->iWidth * ptPixelDatas->iBpp / 8;
    ptPixelDatas->iTotalBytes   = ptPixelDatas->iHeight * ptPixelDatas->iLineBytes;
	if (NULL == ptPixelDatas->aucPixelDatas)
	{
	    ptPixelDatas->aucPixelDatas = malloc(ptPixelDatas->iTotalBytes);
	}

    pucDest = ptPixelDatas->aucPixelDatas;

	/* 循环读取每一行数据并转换 */
	while (tDInfo.output_scanline < tDInfo.output_height) 
	{
        /* 读取一行数据，颜色格式为0xRR, 0xGG, 0xBB */
		(void) jpeg_read_scanlines(&tDInfo, &aucLineBuffer, 1);

		/* 转换到目标格式 */
		CovertOneLine(ptPixelDatas->iWidth, 24, ptPixelDatas->iBpp, aucLineBuffer, pucDest);
		pucDest += ptPixelDatas->iLineBytes;
	}
	
	free(aucLineBuffer);
	jpeg_finish_decompress(&tDInfo);
	jpeg_destroy_decompress(&tDInfo);

    return 0;
}


/**
 * @brief 释放MJPEG转RGB转换缓冲区
 * 
 * @param ptVideoBufOut 输出视频缓冲区
 * @return 0 - 成功
 */
static int Mjpeg2RgbConvertExit(PT_VideoBuf ptVideoBufOut)
{
    if (ptVideoBufOut->tPixelDatas.aucPixelDatas)
    {
        free(ptVideoBufOut->tPixelDatas.aucPixelDatas);
        ptVideoBufOut->tPixelDatas.aucPixelDatas = NULL;
    }
    return 0;
}

/* 定义MJPEG转RGB转换器结构体 */
static T_VideoConvert g_tMjpeg2RgbConvert = {
    .name        = "mjpeg2rgb",
    .isSupport   = isSupportMjpeg2Rgb,
    .Convert     = Mjpeg2RgbConvert,
    .ConvertExit = Mjpeg2RgbConvertExit,
};


/* 注册MJPEG转RGB转换器 */
int Mjpeg2RgbInit(void)
{
    return RegisterVideoConvert(&g_tMjpeg2RgbConvert);
}
