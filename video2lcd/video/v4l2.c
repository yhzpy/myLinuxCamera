#include <config.h>
#include <video_manager.h>
#include <pic_operation.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <poll.h>
/* 流程 */
/ 
 * VIDIOC_QUERYCAP 确定设备是否是视频捕获设备,支持哪种接口(streaming/read,write)
 * VIDIOC_ENUM_FMT 查询支持的像素格式
 * VIDIOC_S_FMT    设置摄像头使用指定格式
 * VIDIOC_REQBUFS  分配buffer
 *对于 streaming:
 * VIDIOC_QUERYBUF 获取每一个buffer的信息 以及 mmap
 * VIDIOC_QBUF     放入队列
 * VIDIOC_STREAMON 启动设备
 * poll            等待数据就绪
 * VIDIOC_DQBUF    从队列中取出
 * 处理....
 * VIDIOC_QBUF     放入队列
 * ....
 *对于read,write:
 *   read
 *   处理....
 *   read
 * VIDIOC_STREAMOFF 停止设备
 *
 */
static int g_aiSupportedFormats[] = {V4L2_PIX_FMT_YUYV, V4L2_PIX_FMT_MJPEG, V4L2_PIX_FMT_RGB565};

static int V4l2GetFrameForReadWrite(PT_VideoDevice ptVideoDevice, PT_VideoBuf ptVideoBuf);
static int V4l2PutFrameForReadWrite(PT_VideoDevice ptVideoDevice, PT_VideoBuf ptVideoBuf);
static T_VideoOpr g_tV4l2VideoOpr;

static int isSupportThisFormat(int iPixelFormat){
    int i;
    for(i = 0; i < sizeof(g_aiSupportedFormats)/sizeof(g_aiSupportedFormats[0]); i++){
        if(iPixelFormat == g_aiSupportedFormats[i]){
            return 1;
        }
    }
    return 0;
}

static int V4l2InitDevice(char *strDevName, PT_VideoDevice ptVideoDevice)
{
    int i;
    int iFd;
    int iError;
    /* 以下5个结构体均来自Linux内核头文件 <linux/videodev2.h>, 是V4L2驱动编程的标准数据结构 */

    struct v4l2_capability tV4l2Cap;         /* 设备能力: 用于VIDIOC_QUERYCAP, 查询设备是否支持视频捕获及streaming/read-write接口 */
	struct v4l2_fmtdesc tFmtDesc;            /* 格式描述: 用于VIDIOC_ENUM_FMT, 枚举设备支持的所有像素格式(YUYV/MJPEG/RGB565等) */
    struct v4l2_format  tV4l2Fmt;            /* 数据格式: 用于VIDIOC_S_FMT, 设置摄像头输出的分辨率(宽x高)和像素格式 */
    struct v4l2_requestbuffers tV4l2ReqBuffs;/* 请求缓冲区: 用于VIDIOC_REQBUFS, 向驱动申请多个帧缓冲区(本项目申请NB_BUFFER=4个) */
    struct v4l2_buffer tV4l2Buf;             /* 单个缓冲区: 用于VIDIOC_QUERYBUF/QBUF/DQBUF, 操作每一个帧缓冲区的索引、状态、mmap偏移等 */

    int iLcdWidth;
    int iLcdHeight;
    int iLcdBpp;

    iFd = open(strDevName, O_RDWR);
    if (iFd < 0)
    {
        DBG_PRINTF("can not open %s\n", strDevName);
        return -1;
    }
    ptVideoDevice->iFd = iFd;/*从这里开始不断完善设备结构体的信息*/
    memset(&tV4l2Cap,0,sizeof(struct v4l2_capability));
    iError = ioctl(iFd, VIOIOC_QUERYCAP, &tV4l2Cap);
    if (iError)
    {
        DBG_PRINTF("ioctl VIOIOC_QUERYCAP failed\n");
        goto err_exit;
    }
    if(!(tV4l2Cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)){
        DBG_PRINTF("this device is not a video capture device\n");
        goto err_exit;
    }
    if(tV4l2Cap.capabilities & V4L2_CAP_STREAMING){
        DBG_PRINTF("this device support streaming interface\n");
    }
    if(tV4l2Cap.capabilities & V4L2_CAP_READWRITE){
        DBG_PRINTF("this device support read/write interfaces \n");
    }

    memset(&tFmtDesc,0,sizeof(struct v4l2_fmtdesc));
    tFmtDesc.index = 0;//根据不同的index 内核返回不同的格式
    tFmtDesc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    while(ioctl(iFd, VIDIOC_ENUM_FMT, &tFmtDesc) == 0){
        if(isSupportedThisFormat(tFmtDesc.pixelformat)){
            ptVideoDevice->iPixelFormat = tFmtDesc.pixelformat;
            break;
        }
        tFmtDesc.index++;
    }

    if(ptVideoDevice->iPixelFormat == NULL){
        DBGPRINTF("this device not support any format\n");
        goto err_exit;
    }

    /* 设置帧格式*/
    /* set format in */
    GetDispResolution(&iLcdWidth, &iLcdHeigt, &iLcdBpp);
    memset(&tV4l2Fmt, 0, sizeof(struct v4l2_format));
    tV4l2Fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    tV4l2Fmt.fmt.pix.pixelformat = ptVideoDevice->iPixelFormat;
    tV4l2Fmt.fmt.pix.width       = iLcdWidth;
    tV4l2Fmt.fmt.pix.height      = iLcdHeigt;
    tV4l2Fmt.fmt.pix.field       = V4L2_FIELD_ANY;

    /* 驱动程序有可能无法支持某些参数(比如分辨率),
     * 驱动程序会修改这些参数, 并返回给应用程序
     */
    iError = ioctl(iFd, VIDIOC_S_FMT, &tV4l2Fmt); //设置格式
    if (iError) 
    {
    	DBG_PRINTF("Unable to set format\n");
        goto err_exit;        
    }
    ptVideoDevice->iWidth  = tV4l2Fmt.fmt.pix.width;
    ptVideoDevice->iHeight = tV4l2Fmt.fmt.pix.height;

    /* ====================================================================
     * 第四步: 申请帧缓冲区 (VIDIOC_REQBUFS)
     * 作用: 向内核驱动申请多个缓冲区, 用于后续存放摄像头采集的每一帧图像数据
     * 类比: 相当于向餐厅预订N个盘子, 等菜做好了轮流用这些盘子端菜
     * ==================================================================== */
    memset(&tV4l2ReqBuffs, 0, sizeof(struct v4l2_requestbuffers));
    tV4l2ReqBuffs.count = NB_BUFFER;                      /* 申请NB_BUFFER(4)个缓冲区 */
    tV4l2ReqBuffs.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;     /* 缓冲区用于视频采集 */
    tV4l2ReqBuffs.memory = V4L2_MEMORY_MMAP;              /* 使用mmap方式映射到用户空间 */

    iError = ioctl(iFd, VIDIOC_REQBUFS, &tV4l2ReqBuffs);//申请缓冲区
    if (iError) 
    {
        DBG_PRINTF("Unable to allocate buffers.\n");
        goto err_exit;        
    }
    
    /* 内核可能分配的实际缓冲区数量与请求的不同, 以实际返回值为准 */
    ptVideoDevice->iVideoBufCnt = tV4l2ReqBuffs.count;

    /* ====================================================================
     * 根据设备能力, 分两种模式处理:
     * - Streaming模式: 内核分配缓冲区 → mmap映射 → 入队 → 后续用DQBUF/QBUF轮转
     * - ReadWrite模式: 用户自己malloc缓冲区 → 后续用read()读取数据
     * ==================================================================== */
}









































/* 构造一个VideoOpr结构体 */
static T_VideoOpr g_tV4l2VideoOpr = {
    .name        = "v4l2",
    .InitDevice  = V4l2InitDevice,
    .ExitDevice  = V4l2ExitDevice,
    .GetFrame    = V4l2GetFrameForStreaming,
    .PutFrame    = V4l2PutFrameForStreaming,
    .StartDevice = V4l2StartDevice,
    .StopDevice  = V4l2StopDevice,
};

/* 注册这个结构体 */
int V4l2Init(void)
{
    return RegisterVideoOpr(&g_tV4l2VideoOpr);
}