#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <config.h>
#include <disp_manager.h>
#include <video_manager.h>
#include <convert_manager.h>
#include <render.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>

/**
 * @file main.c
 * @brief 嵌入式Linux相机主程序
 * 
 * 功能说明：
 * 1. 打开摄像头设备（/dev/videoX）
 * 2. 初始化LCD显示设备（framebuffer）
 * 3. 循环采集摄像头图像数据
 * 4. 将图像数据转换为LCD支持的格式
 * 5. 如果图像尺寸大于LCD，进行缩放处理
 * 6. 将图像居中显示在LCD屏幕上
 * 
 * 使用方法：./video2lcd </dev/video0,1,...>
 */

/* video2lcd </dev/video0,1,...> */

int main(int argc, char *argv)
{
	int iError;
    T_VideoDevice tVideoDevice;      /* 摄像头设备结构体 */
    PT_VideoConvert ptVideoConvert;  /* 视频格式转换器指针 */
    int iPixelFormatOfVideo;         /* 摄像头输出的像素格式 */
    int iPixelFormatOfDisp;          /* LCD显示的像素格式 */

    PT_VideoBuf ptVideoBufCur;       /* 当前处理的视频缓冲区 */
    T_VideoBuf tVideoBuf;            /* 从摄像头获取的原始图像缓冲区 */
    T_VideoBuf tConvertBuf;          /* 格式转换后的图像缓冲区 */
    T_VideoBuf tZoomBuf;             /* 缩放后的图像缓冲区 */
    T_VideoBuf tFrameBuf;            /* 帧缓冲区（直接映射到LCD） */
    
    int iLcdWidth;                   /* LCD宽度（像素） */
    int iLcdHeigt;                   /* LCD高度（像素） */
    int iLcdBpp;                     /* LCD每像素位数（bits per pixel） */

    int iTopLeftX;                   /* 图像显示起始X坐标（居中计算） */
    int iTopLeftY;                   /* 图像显示起始Y坐标（居中计算） */

    float k;                         /* 图像宽高比，用于等比例缩放计算 */

    if(argc < 2)
    {
        printf("Usage: %s <dev/video0,1,...>\n>", argv[0]);
        return -1;
    }

    DisplayInit();   /*main->disp_manager注册fb驱动->fb.c 根据里面的函数注册结构体*/

    SelectAndInitDefaultDispDev("fb");  /*拿到display结构体 执行里面的init  和  清屏 clean*/

    GetDispResolution(&iLcdWidth, &iLcdHeigt, &iLcdBpp);  /*下面俩函数和disp结构体同文件 所以main不用显式返回结构体 也能调用他们*/

    GetVideoBufForDisplay(&tFrameBuf);/*tFrameBuf有buf映射到用户区的首地址*/
    iPiexelFormatOfDisp = tFrameBuf.iPixelFormat;/*LCD的像素格式*/

    VideoInit();  /*main->video_manager注册驱动->v4l2.c 根据里面的函数注册结构体 返回给video_manager 后面直接调用video_manager*/

    iError = VideoDeviceInit(argv[1], &tVideoDevice);  /*ioctl 填写VideoDevice数据*/
    if(iError)
    {
        DBG_PRINTF("VideoDeviceInit error: %d\n", iError);
        return -1;
    }

    iPixelFormatOfVideo = tVideoDevice.ptOPr->GetFormat(&tVideoDevice);

    VideoConvertInit();

    ptVideoConvert = GetVideoConvertForFormats(iPixelFormatOfVideo, iPixelFormatOfDisp);
    /*摄像头的像素格式和lcd不一致的话 需要转变*/
    if(NULL == ptVideoConvert)
    {
        DBG_PRINTF("No suitable video convert for formats: %d -> %d\n", iPixelFormatOfVideo, iPixelFormatOfDisp);
        return -1;
    }

    /* 启动摄像头设备，开始图像采集 */
    iError = tVideoDevice.ptOPr->StartDevice(&tVideoDevice);
    if (iError)
    {
        DBG_PRINTF("StartDevice for %s error!\n", argv[1]);
        return -1;
    }

    /* 初始化各缓冲区结构体 */
    memset(&tVideoBuf, 0, sizeof(tVideoBuf));
    memset(&tConvertBuf, 0, sizeof(tConvertBuf));
    tConvertBuf.iPixelFormat     = iPixelFormatOfDisp;
    tConvertBuf.tPixelDatas.iBpp = iLcdBpp;
    
    memset(&tZoomBuf, 0, sizeof(tZoomBuf));

    /*主循环*/
    while(1)
    {
        iError = tVideoDevice.ptOPr->GetFrame(&tVideoDevice, &tVideoBuf);
        /*videobuf是应用层抽象 和tv4l2buf 从内核mmap过来的是同一块内存*/
        if(iError)
        {
            DBG_PRINTF("GetFrame error: %d\n", iError);
            continue;
        }
        ptVideoBufCur = &tVideoBuf;

        if(iPixelFormatOfVideo != iPixelFormatOfDisp)
        {
            iError = ptVideoConvert->Convert(&tVideoBuf, &tConvertBuf);
            DBG_PRINTF("Convert: %d -> %d\n", iPixelFormatOfVideo, iPixelFormatOfDisp);
            if(iError)
            {
                DBG_PRINTF("Convert error: %d\n", iError);
                return -1;
            }
            ptVideoBufCur = &tConvertBuf;
        }

        /* 步骤3：如果图像尺寸大于LCD分辨率，进行等比例缩放 */
        if (ptVideoBufCur->tPixelDatas.iWidth > iLcdWidth || ptVideoBufCur->tPixelDatas.iHeight > iLcdHeigt)
        {
            /* 计算缩放后的尺寸，保持原始宽高比 */
            /* 缩放策略：以LCD宽度为基准，按比例计算高度；如果高度超出则反过来计算 */
            k = (float)ptVideoBufCur->tPixelDatas.iHeight / ptVideoBufCur->tPixelDatas.iWidth;
            tZoomBuf.tPixelDatas.iWidth  = iLcdWidth;
            tZoomBuf.tPixelDatas.iHeight = iLcdWidth * k;
            if ( tZoomBuf.tPixelDatas.iHeight > iLcdHeigt)
            {
                tZoomBuf.tPixelDatas.iWidth  = iLcdHeigt / k;
                tZoomBuf.tPixelDatas.iHeight = iLcdHeigt;
            }
            tZoomBuf.tPixelDatas.iBpp        = iLcdBpp;
            tZoomBuf.tPixelDatas.iLineBytes  = tZoomBuf.tPixelDatas.iWidth * tZoomBuf.tPixelDatas.iBpp / 8;
            tZoomBuf.tPixelDatas.iTotalBytes = tZoomBuf.tPixelDatas.iLineBytes * tZoomBuf.tPixelDatas.iHeight;

            /* 首次分配缩放缓冲区内存 */
            if (!tZoomBuf.tPixelDatas.aucPixelDatas)
            {
                tZoomBuf.tPixelDatas.aucPixelDatas = malloc(tZoomBuf.tPixelDatas.iTotalBytes);
            }
            
            /* 执行图像缩放（使用最近邻插值算法） */
            PicZoom(&ptVideoBufCur->tPixelDatas, &tZoomBuf.tPixelDatas);
            ptVideoBufCur = &tZoomBuf;
        }

        /* 步骤4：计算图像在LCD上的居中显示位置 */
        /* 将图像合并到帧缓冲区，从计算出的左上角坐标开始绘制 */
        iTopLeftX = (iLcdWidth - ptVideoBufCur->tPixelDatas.iWidth) / 2;
        iTopLeftY = (iLcdHeigt - ptVideoBufCur->tPixelDatas.iHeight) / 2;

        /* 将缩放后的图像数据复制到帧缓冲区的指定位置 */
        PicMerge(iTopLeftX, iTopLeftY, &ptVideoBufCur->tPixelDatas, &tFrameBuf.tPixelDatas);

        /* 将帧缓冲区数据刷新到LCD硬件，完成显示 */
        FlushPixelDatasToDev(&tFrameBuf.tPixelDatas);

        /* 步骤5：将视频缓冲区归还给摄像头驱动，用于下一帧采集 */
        iError = tVideoDevice.ptOPr->PutFrame(&tVideoDevice, &tVideoBuf);
        if (iError)
        {
            DBG_PRINTF("PutFrame for %s error!\n", argv[1]);
            return -1;
        }                    

        /* 循环继续，回到步骤1获取下一帧 */
    }

    return 0;
}