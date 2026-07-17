#include <config.h>
#include <video_manager.h>
#include <pic_operation.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <poll.h>
/* 流程 */

/* VIDIOC_QUERYCAP 确定设备是否是视频捕获设备,支持哪种接口(streaming/read,write)
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
static int g_aiSupportedFormats[] = {V4L2_PIX_FMT_MJPEG,V4L2_PIX_FMT_YUYV,V4L2_PIX_FMT_RGB565};

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
    iError = ioctl(iFd, VIDIOC_QUERYCAP, &tV4l2Cap);
    if (iError)
    {
        DBG_PRINTF("ioctl VIDIOC_QUERYCAP failed\n");
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
        if(isSupportThisFormat(tFmtDesc.pixelformat)){
            ptVideoDevice->iPixelFormat = tFmtDesc.pixelformat;
            break;
        }
        tFmtDesc.index++;
    }

    if(ptVideoDevice->iPixelFormat == 0){
        DBG_PRINTF("this device not support any format\n");
        goto err_exit;
    }

    /* 设置帧格式*/
    /* set format in */
    GetDispResolution(&iLcdWidth, &iLcdHeight, &iLcdBpp);
    memset(&tV4l2Fmt, 0, sizeof(struct v4l2_format));
    tV4l2Fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    tV4l2Fmt.fmt.pix.pixelformat = ptVideoDevice->iPixelFormat;
    tV4l2Fmt.fmt.pix.width       = iLcdWidth;
    tV4l2Fmt.fmt.pix.height      = iLcdHeight;
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
    if (tV4l2Cap.capabilities & V4L2_CAP_STREAMING)
    {
        /* ---------------------------------------------------------------
         * Streaming模式 - 阶段1: 查询并映射每个缓冲区
         * 对每个内核分配的缓冲区:
         *   1. VIDIOC_QUERYBUF: 查询缓冲区在内核中的偏移地址和大小
         *   2. mmap: 将该内核缓冲区映射到用户空间, 得到用户态指针
         * 映射后, 用户空间指针直接指向内核缓冲区, 零拷贝
         * --------------------------------------------------------------- */
        for (i = 0; i < ptVideoDevice->iVideoBufCnt; i++) 
        {
            memset(&tV4l2Buf, 0, sizeof(struct v4l2_buffer));
            tV4l2Buf.index = i;                           /* 指定要查询第i个缓冲区 */
            tV4l2Buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            tV4l2Buf.memory = V4L2_MEMORY_MMAP;

            /* VIDIOC_QUERYBUF: 内核返回该缓冲区的length(大小)和m.offset(映射偏移) */
            iError = ioctl(iFd, VIDIOC_QUERYBUF, &tV4l2Buf);
            if (iError) 
            {
                DBG_PRINTF("Unable to query buffer.\n");
                goto err_exit;
            }

            ptVideoDevice->iVideoBufMaxLen = tV4l2Buf.length;  /* 记录单个缓冲区大小 */

            /* mmap映射: 将内核缓冲区映射到用户空间
             * 参数说明:
             *   - 第一个参数0: 让内核选择映射地址(起始地址)
             *   - tV4l2Buf.length: 映射长度
             *   - PROT_READ: 只读映射(用户空间只读, 数据由内核写入)
             *   - MAP_SHARED: 共享映射, 内核和用户空间看到同一块物理内存
             *   - iFd: 设备文件描述符
             *   - tV4l2Buf.m.offset: 该缓冲区在内核中的偏移量(由QUERYBUF返回)
             */
            ptVideoDevice->pucVideBuf[i] = mmap(0,
                    tV4l2Buf.length, PROT_READ, MAP_SHARED, iFd,
                    tV4l2Buf.m.offset);//每一次偏移量都不同
            if (ptVideoDevice->pucVideBuf[i] == MAP_FAILED) 
            {
                DBG_PRINTF("Unable to map buffer\n");
                goto err_exit;
            }
        }        

        /* ---------------------------------------------------------------
         * Streaming模式 - 阶段2: 将所有缓冲区入队(VIDIOC_QBUF)
         * 作用: 把映射好的缓冲区全部放入内核的"输入队列"
         *       告诉驱动: "这些缓冲区都空着, 你可以往里面写数据了"
         * 后续流程: 驱动采集一帧 → 放入队列 → 用户DQBUF取出 → 处理 → QBUF放回
         * 类比: 把空盘子全部放到传菜口, 等厨师做好了端走
         * --------------------------------------------------------------- */
        for (i = 0; i < ptVideoDevice->iVideoBufCnt; i++) 
        {
            memset(&tV4l2Buf, 0, sizeof(struct v4l2_buffer));
            tV4l2Buf.index = i;
            tV4l2Buf.type  = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            tV4l2Buf.memory = V4L2_MEMORY_MMAP;

            /* VIDIOC_QBUF: 将第i个缓冲区入队, 交给驱动使用 */
            iError = ioctl(iFd, VIDIOC_QBUF, &tV4l2Buf);
            if (iError)
            {
                DBG_PRINTF("Unable to queue buffer.\n");
                goto err_exit;
            }
        }
        
    }
    else if (tV4l2Cap.capabilities & V4L2_CAP_READWRITE)
    {
        /* ---------------------------------------------------------------
         * ReadWrite模式: 不使用内核缓冲区, 用户空间自己malloc
         * 后续通过read(fd, buf, size)直接读取一帧数据到用户缓冲区
         * 这种方式效率较低, 但不需要mmap和队列管理
         * --------------------------------------------------------------- */
        g_tV4l2VideoOpr.GetFrame = V4l2GetFrameForReadWrite;   /* 设置read方式的取帧函数 */
        g_tV4l2VideoOpr.PutFrame = V4l2PutFrameForReadWrite;   /* 设置read方式的还帧函数 */
        
        /* ReadWrite模式只需要1个缓冲区即可, 读一帧处理一帧 */
        ptVideoDevice->iVideoBufCnt  = 1;

        /* 计算缓冲区大小: 宽 × 高 × 每像素字节数
         * 因为不确定具体像素格式, 保守估计每像素4字节(如RGBA)
         */
        ptVideoDevice->iVideoBufMaxLen = ptVideoDevice->iWidth * ptVideoDevice->iHeight * 4;

        /* 在用户空间分配缓冲区, 后续read()时数据写入这里 */
        ptVideoDevice->pucVideBuf[0] = malloc(ptVideoDevice->iVideoBufMaxLen);
    }

    
    return 0;
    
err_exit:    
    close(iFd);
    return -1;    
}

static int V4l2ExitDevice(PT_VideoDevice ptVideoDevice)
{
    int i;
    for (i = 0; i < ptVideoDevice->iVideoBufCnt; i++)
    {
        if (ptVideoDevice->pucVideBuf[i])
        {
            munmap(ptVideoDevice->pucVideBuf[i], ptVideoDevice->iVideoBufMaxLen);
            ptVideoDevice->pucVideBuf[i] = NULL;
        }
    }
        
    close(ptVideoDevice->iFd);
    return 0;
}
    
static int V4l2GetFrameForStreaming(PT_VideoDevice ptVideoDevice, PT_VideoBuf ptVideoBuf)
{
    struct pollfd tFds[1];
    int iRet;
    struct v4l2_buffer tV4l2Buf;
            
    /* poll */
    tFds[0].fd     = ptVideoDevice->iFd;
    tFds[0].events = POLLIN;

    iRet = poll(tFds, 1, -1);
    if (iRet <= 0)
    {
        DBG_PRINTF("poll error!\n");
        return -1;
    }
    
    /* VIDIOC_DQBUF */
    memset(&tV4l2Buf, 0, sizeof(struct v4l2_buffer));
    tV4l2Buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    tV4l2Buf.memory = V4L2_MEMORY_MMAP;
    iRet = ioctl(ptVideoDevice->iFd, VIDIOC_DQBUF, &tV4l2Buf);
    if (iRet < 0) 
    {
    	DBG_PRINTF("Unable to dequeue buffer.\n");
    	return -1;
    }
    ptVideoDevice->iVideoBufCurIndex = tV4l2Buf.index;

    ptVideoBuf->iPixelFormat        = ptVideoDevice->iPixelFormat;
    ptVideoBuf->tPixelDatas.iWidth  = ptVideoDevice->iWidth;
    ptVideoBuf->tPixelDatas.iHeight = ptVideoDevice->iHeight;
    ptVideoBuf->tPixelDatas.iBpp    = (ptVideoDevice->iPixelFormat == V4L2_PIX_FMT_YUYV) ? 16 : 
                                        (ptVideoDevice->iPixelFormat == V4L2_PIX_FMT_MJPEG) ? 24 :  
                                        (ptVideoDevice->iPixelFormat == V4L2_PIX_FMT_RGB565) ? 16 : 0;
    ptVideoBuf->tPixelDatas.iLineBytes    = ptVideoDevice->iWidth * ptVideoBuf->tPixelDatas.iBpp / 8;
    ptVideoBuf->tPixelDatas.iTotalBytes   = tV4l2Buf.bytesused;
    ptVideoBuf->tPixelDatas.aucPixelDatas = ptVideoDevice->pucVideBuf[tV4l2Buf.index];    
    return 0;
}


static int V4l2PutFrameForStreaming(PT_VideoDevice ptVideoDevice, PT_VideoBuf ptVideoBuf)
{
    /* VIDIOC_QBUF */
    struct v4l2_buffer tV4l2Buf;
    int iError;
    
	memset(&tV4l2Buf, 0, sizeof(struct v4l2_buffer));
	tV4l2Buf.index  = ptVideoDevice->iVideoBufCurIndex;
	tV4l2Buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	tV4l2Buf.memory = V4L2_MEMORY_MMAP;
	iError = ioctl(ptVideoDevice->iFd, VIDIOC_QBUF, &tV4l2Buf);
	if (iError) 
    {
	    DBG_PRINTF("Unable to queue buffer.\n");
	    return -1;
	}
    return 0;
}

static int V4l2GetFrameForReadWrite(PT_VideoDevice ptVideoDevice, PT_VideoBuf ptVideoBuf)
{
    int iRet;

    iRet = read(ptVideoDevice->iFd, ptVideoDevice->pucVideBuf[0], ptVideoDevice->iVideoBufMaxLen);
    if (iRet <= 0)
    {
        return -1;
    }
    
    ptVideoBuf->iPixelFormat        = ptVideoDevice->iPixelFormat;
    ptVideoBuf->tPixelDatas.iWidth  = ptVideoDevice->iWidth;
    ptVideoBuf->tPixelDatas.iHeight = ptVideoDevice->iHeight;
    ptVideoBuf->tPixelDatas.iBpp    = (ptVideoDevice->iPixelFormat == V4L2_PIX_FMT_YUYV) ? 16 : 
                                        (ptVideoDevice->iPixelFormat == V4L2_PIX_FMT_MJPEG) ? 24 :  
                                        (ptVideoDevice->iPixelFormat == V4L2_PIX_FMT_RGB565) ? 16 : 0;
    ptVideoBuf->tPixelDatas.iLineBytes    = ptVideoDevice->iWidth * ptVideoBuf->tPixelDatas.iBpp / 8;
    ptVideoBuf->tPixelDatas.iTotalBytes   = iRet;
    ptVideoBuf->tPixelDatas.aucPixelDatas = ptVideoDevice->pucVideBuf[0];    
    
    return 0;
}


static int V4l2PutFrameForReadWrite(PT_VideoDevice ptVideoDevice, PT_VideoBuf ptVideoBuf)
{
    return 0;
}

static int V4l2StartDevice(PT_VideoDevice ptVideoDevice)
{
    int iType = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    int iError;

    iError = ioctl(ptVideoDevice->iFd, VIDIOC_STREAMON, &iType);
    if (iError) 
    {
    	DBG_PRINTF("Unable to start capture.\n");
    	return -1;
    }
    return 0;
}
    
static int V4l2StopDevice(PT_VideoDevice ptVideoDevice)
{
    int iType = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    int iError;

    iError = ioctl(ptVideoDevice->iFd, VIDIOC_STREAMOFF, &iType);
    if (iError) 
    {
    	DBG_PRINTF("Unable to stop capture.\n");
    	return -1;
    }
    return 0;
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