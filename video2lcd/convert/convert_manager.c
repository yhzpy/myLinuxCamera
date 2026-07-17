/**
 * @file convert_manager.c
 * @brief 视频格式转换管理实现文件
 * 
 * 实现视频格式转换器的注册、选择等功能
 * 支持多种格式转换器的动态注册
 */

#include <config.h>
#include <convert_manager.h>
#include <string.h>

/* 全局变量：视频格式转换器链表头 */
static PT_VideoConvert g_ptVideoConvertHead = NULL;

/**********************************************************************
 * 函数名称: RegisterVideoConvert
 * 功能描述: 注册"转换模块"，所谓转换模块就是获取图像数据并进行格式转换的函数集合
 * 输入参数: ptVideoConvert - 一个结构体指针，含"格式转换"的操作接口
 * 输出参数: 无
 * 返 回 值: 0 - 成功, 其他值 - 失败
 ***********************************************************************/
int RegisterVideoConvert(PT_VideoConvert ptVideoConvert)
{
	PT_VideoConvert ptTmp;

	if (!g_ptVideoConvertHead)
	{
		g_ptVideoConvertHead   = ptVideoConvert;
		ptVideoConvert->ptNext = NULL;
	}
	else
	{
		ptTmp = g_ptVideoConvertHead;
		while (ptTmp->ptNext)
		{
			ptTmp = ptTmp->ptNext;
		}
		ptTmp->ptNext     = ptVideoConvert;
		ptVideoConvert->ptNext = NULL;
	}

	return 0;
}


/**********************************************************************
 * 函数名称: ShowVideoConvert
 * 功能描述: 显示所有支持的"转换模块"列表
 * 输入参数: 无
 * 输出参数: 无
 * 返 回 值: 无
 ***********************************************************************/
void ShowVideoConvert(void)
{
	int i = 0;
	PT_VideoConvert ptTmp = g_ptVideoConvertHead;

	while (ptTmp)
	{
		printf("%02d %s\n", i++, ptTmp->name);
		ptTmp = ptTmp->ptNext;
	}
}

/**********************************************************************
 * 函数名称: GetVideoConvert
 * 功能描述: 根据名称获取指定的"转换模块"
 * 输入参数: pcName - 转换模块名称
 * 输出参数: 无
 * 返 回 值: NULL   - 失败,没有指定名称的模块, 
 *           非NULL - 转换模块的PT_VideoConvert结构体指针
 ***********************************************************************/
PT_VideoConvert GetVideoConvert(char *pcName)
{
	PT_VideoConvert ptTmp = g_ptVideoConvertHead;
	
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

/**
 * @brief 根据输入输出格式获取合适的转换器
 * 
 * 遍历所有注册的转换器，找到支持指定格式转换的转换器
 * 
 * @param iPixelFormatIn 输入像素格式
 * @param iPixelFormatOut 输出像素格式
 * @return 转换器指针，NULL表示没有支持的转换器
 */
PT_VideoConvert GetVideoConvertForFormats(int iPixelFormatIn, int iPixelFormatOut)
{
	PT_VideoConvert ptTmp = g_ptVideoConvertHead;
	
	while (ptTmp)
	{
        if (ptTmp->isSupport(iPixelFormatIn, iPixelFormatOut))
        {
            return ptTmp;
        }
		ptTmp = ptTmp->ptNext;
	}
	return NULL;
}


/**********************************************************************
 * 函数名称: VideoConvertInit
 * 功能描述: 调用各个转换模块的初始化函数
 * 输入参数: 无
 * 输出参数: 无
 * 返 回 值: 0 - 成功, 其他值 - 失败
 ***********************************************************************/
int VideoConvertInit(void)
{
	int iError;

    iError = Yuv2RgbInit();      /* 初始化YUV转RGB转换器 */
    iError |= Mjpeg2RgbInit();   /* 初始化MJPEG转RGB转换器 */
    iError |= Rgb2RgbInit();     /* 初始化RGB转RGB转换器 */

	return iError;
}
