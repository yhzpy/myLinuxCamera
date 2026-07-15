/**
 * @file fb.c
 * @brief Framebuffer显示设备实现文件
 * 
 * 实现基于Linux Framebuffer的显示设备驱动
 * 通过/dev/fb0设备文件操作LCD屏幕
 */

#include <config.h>
#include <disp_manager.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/fb.h>
#include <string.h>

/* 前置声明内部函数 */
static int FBDeviceInit(void);
static int FBShowPixel(int iX, int iY, unsigned int dwColor);
static int FBCleanScreen(unsigned int dwBackColor);
static int FBShowPage(PT_PixelDatas ptPixelDatas);

/* 全局变量 */
static int g_fd;                                              /* framebuffer设备文件描述符 */

static struct fb_var_screeninfo g_tFBVar;                     /* framebuffer可变信息 */
static struct fb_fix_screeninfo g_tFBFix;                     /* framebuffer固定信息 */
static unsigned char *g_pucFBMem;                             /* framebuffer映射的内存地址 */
static unsigned int g_dwScreenSize;                           /* 屏幕总字节数 */

static unsigned int g_dwLineWidth;                            /* 每行字节数 */
static unsigned int g_dwPixelWidth;                           /* 每像素字节数 */

/* Framebuffer显示操作接口结构体 */
static T_DispOpr g_tFBOpr = {
	.name        = "fb",              /* 设备名称 */
	.DeviceInit  = FBDeviceInit,      /* 初始化函数 */
	.ShowPixel   = FBShowPixel,       /* 画点函数 */
	.CleanScreen = FBCleanScreen,     /* 清屏函数 */
	.ShowPage    = FBShowPage,        /* 显示页函数 */
};

/**********************************************************************
 * 函数名称: FBDeviceInit
 * 功能描述: "framebuffer显示设备"的初始化函数
 *           打开/dev/fb0设备，获取屏幕信息并映射到内存
 * 输入参数: 无
 * 输出参数: 无
 * 返 回 值: 0 - 成功, 其他值 - 失败
 ***********************************************************************/
static int FBDeviceInit(void)
{
     int ret;
     g_fd = open(FB_DEVICE_NAME, O_RDWR);
     if(g_fd < 0)
     {
        DBG_PRINTF("cant open %s \n", FB_DEVICE_NAME);
        return -1;
     }
     /* 获取屏幕信息并填写到实例中 */
     ret = ioctl(g_fd,FBIOGET_VSCREENINFO,&g_tFBVar);
     if(ret < 0)
     {
        DBG_PRINTF("ioctl error \n");
        return -1;
     }
     ret = ioctl(g_fd,FBIOGET_FSCREENINFO,&g_tFBFix);
     if(ret < 0)
     {
        DBG_PRINTF("ioctl error \n");
        return -1;
     }
     g_dwScreenSize = g_tFBVar.xres * g_tFBVar.yres * g_tFBVar.bits_per_pixel / 8;
     g_pucFBMem = mmap(NULL,g_dwScreenSize, PROT_READ | PROT_WRITE, MAP_SHARED, g_fd, 0);
     if(0> g_pucFBMem)
     {
        DBG_PRINTF("mmap error \n");
        return -1;
     }

     /*填充到操作接口的结构体*/
     g_tFBOpr.iXres       = g_tFBVar.xres;
	 g_tFBOpr.iYres       = g_tFBVar.yres;
	 g_tFBOpr.iBpp        = g_tFBVar.bits_per_pixel;
	 g_tFBOpr.iLineWidth  = g_tFBVar.xres * g_tFBOpr.iBpp / 8;
	 g_tFBOpr.pucDispMem  = g_pucFBMem;

	 g_dwLineWidth  = g_tFBVar.xres * g_tFBVar.bits_per_pixel / 8;
	 g_dwPixelWidth = g_tFBVar.bits_per_pixel / 8;
	
	 return 0;
}

/*算到那个点的内存地址 替换成颜色值*/
/**********************************************************************
 * 函数名称: FBShowPixel
 * 功能描述: 在FrameBuffer的指定坐标画点为某颜色
 * 输入参数: iX - 画点的X坐标
 *           iY - 画点的Y坐标
 *           dwColor - 颜色值，格式为32Bpp，如0x00RRGGBB
 * 输出参数: 无
 * 返 回 值: 0 - 成功, 其他值 - 失败
 ***********************************************************************/
static int FBShowPixel(int iX, int iY, unsigned int dwColor)
{
    unsigned char *pucFB;
    unsigned short *pwFB16bpp;
    unsigned int *pwFB32bpp;
    unsigned short wColor16bpp;
    int iRed;
    int iGreen;
    int iBlue;

    if((iX > g_tFBVar.xres) || (iY > g_tFBVar.yres))
    {
        DBG_PRINTF("coordinate error \n");
        return -1;
    }
    pucFB = g_pucFBMem + iY * g_dwLinewidth + iX * g_dwPixelWidth;
    pwFB16bpp = (unsigned short *)pucFB;
    pwFB32bpp = (unsigned int *)pucFB;

    switch(g_tFBVar.bits_per_pixel)
    {
        case 8:
        {
            *pucFB = (unsigned char)dwColor;/*指向转换成1字节的颜色值*/
            break;
        }
		case 16:
		{
			/* 从dwColor中提取RGB原色，
			 * 转换为16Bpp的RGB565格式颜色
			 */
			iRed   = (dwColor >> (16+3)) & 0x1f;
			iGreen = (dwColor >> (8+2)) & 0x3f;
			iBlue  = (dwColor >> 3) & 0x1f;
			wColor16bpp = (iRed << 11) | (iGreen << 5) | iBlue;
			*pwFB16bpp	= wColor16bpp;
			break;
		}
		case 32:
		{
			*pdwFB32bpp = dwColor;
			break;
		}
		default :
		{
			DBG_PRINTF("can't support %d bpp\n", g_tFBVar.bits_per_pixel);
			return -1;
		}
	}

	return 0;
}

/**********************************************************************
 * 函数名称: FBShowPage
 * 功能描述: 将PT_PixelDatas中的颜色数据输出到FrameBuffer中显示出来
 * 输入参数: ptPixelDatas - 含要显示像素数据的结构体
 * 输出参数: 无
 * 返 回 值: 0 - 成功, 其他值 - 失败
 ***********************************************************************/
static int FBShowPage(PT_PixelDatas ptPixelDatas)
{
    if(g_tFBOpr.pucDispMem != ptPixelDatas->aucPixelDates)
    {
        memcpy(g_tFBOpr.pucDispMem, ptPixelDatas->aucPixelDatas, ptPixelDatas->iTotalBytes);
    }

    return 0;
}

/**********************************************************************
 * 函数名称: FBCleanScreen
 * 功能描述: "framebuffer显示设备"清屏函数
 * 输入参数: dwBackColor - 清屏后的背景颜色
 * 输出参数: 无
 * 返 回 值: 0 - 成功, 其他值 - 失败
 ***********************************************************************/
static int FBCleanScreen(unsigned int dwBackColor)
{
	unsigned char *pucFB;
	unsigned short *pwFB16bpp;
	unsigned int *pdwFB32bpp;
	unsigned short wColor16bpp; /* 565格式 */
	int iRed;
	int iGreen;
	int iBlue;
	int i = 0;

	pucFB      = g_pucFBMem;
	pwFB16bpp  = (unsigned short *)pucFB;
	pdwFB32bpp = (unsigned int *)pucFB;

	switch (g_tFBVar.bits_per_pixel)
	{
		case 8:
		{
			memset(g_pucFBMem, dwBackColor, g_dwScreenSize);
			break;
		}
		case 16:
		{
			/* 从dwBackColor中提取RGB原色，
			 * 转换为16Bpp的RGB565格式颜色
			 */
			iRed   = (dwBackColor >> (16+3)) & 0x1f;
			iGreen = (dwBackColor >> (8+2)) & 0x3f;
			iBlue  = (dwBackColor >> 3) & 0x1f;
			wColor16bpp = (iRed << 11) | (iGreen << 5) | iBlue;
			while (i < g_dwScreenSize)
			{
				*pwFB16bpp	= wColor16bpp;
				pwFB16bpp++;
				i += 2;
			}
			break;
		}
		case 32:
		{
			while (i < g_dwScreenSize)
			{
				*pdwFB32bpp	= dwBackColor;
				pdwFB32bpp++;
				i += 4;
			}
			break;
		}
		default :
		{
			DBG_PRINTF("can't support %d bpp\n", g_tFBVar.bits_per_pixel);
			return -1;
		}
	}

	return 0;
}

/**********************************************************************
 * 函数名称: FBInit
 * 功能描述: 注册"framebuffer显示设备"
 * 输入参数: 无
 * 输出参数: 无
 * 返 回 值: 0 - 成功, 其他值 - 失败
 ***********************************************************************/
int FBInit(void)
{
	return RegisterDispOpr(&g_tFBOpr);
}
