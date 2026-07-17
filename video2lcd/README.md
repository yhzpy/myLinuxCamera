# 嵌入式Linux相机项目

## 项目简介

本项目是一个基于嵌入式Linux系统的相机应用，通过V4L2接口采集摄像头数据，并将图像实时显示在LCD屏幕上。

### 主要功能

- 支持多种摄像头格式（YUV422、MJPEG、RGB565）
- 自动格式转换（YUV->RGB、MJPEG->RGB、RGB->RGB）
- 图像自动缩放（当摄像头分辨率大于LCD时）
- 图像居中显示
- 基于Framebuffer的显示驱动

---

## 项目结构

```
video2lcd/
├── main.c                    # 主程序入口
├── netprint_client.c         # 网络打印客户端
│
├── include/                  # 头文件目录
│   ├── config.h              # 项目配置
│   ├── pic_operation.h       # 图像像素数据操作
│   ├── video_manager.h       # 视频设备管理
│   ├── disp_manager.h        # 显示设备管理
│   ├── convert_manager.h     # 格式转换管理
│   └── render.h              # 图像渲染操作
│
├── video/                    # 视频采集模块
│   ├── video_manager.c       # 视频设备管理实现
│   └── v4l2.c                # V4L2驱动实现
│
├── display/                  # 显示模块
│   ├── disp_manager.c        # 显示设备管理实现
│   └── fb.c                  # Framebuffer驱动实现
│
├── convert/                  # 格式转换模块
│   ├── convert_manager.c     # 转换管理实现
│   ├── yuv2rgb.c             # YUV转RGB
│   ├── mjpeg2rgb.c           # MJPEG转RGB
│   ├── rgb2rgb.c             # RGB转RGB
│   ├── color.c               # 颜色空间转换
│   └── color.h               # 颜色转换头文件
│
├── render/                   # 渲染模块
│   └── operation/
│       ├── zoom.c            # 图像缩放
│       └── merge.c           # 图像合并
│
└── Makefile                  # 项目Makefile
```

---

## 模块说明

### 1. 主程序模块 (main.c)

**功能**：程序入口，协调各模块工作

**主要流程**：
1. 初始化显示设备（Framebuffer）
2. 初始化视频采集设备（V4L2）
3. 初始化格式转换器
4. 循环采集->转换->缩放->显示

### 2. 视频采集模块 (video/)

**功能**：从摄像头采集图像数据

**核心结构体**：
- `T_VideoDevice`：视频设备信息
- `T_VideoOpr`：视频操作接口
- `T_VideoBuf`：视频缓冲区

**支持格式**：
- V4L2_PIX_FMT_YUYV (YUV422)
- V4L2_PIX_FMT_MJPEG (MJPEG)
- V4L2_PIX_FMT_RGB565 (RGB565)

**数据获取方式**：
- Streaming I/O（内存映射，高效）
- Read/Write（直接读取，兼容）

### 3. 显示模块 (display/)

**功能**：将图像数据显示到LCD屏幕

**核心结构体**：
- `T_DispOpr`：显示操作接口
- `T_VideoMem`：视频内存管理
- `T_Layout`：显示区域布局

**功能特性**：
<!-- - 支持多显示设备注册
- VideoMem缓冲机制加速显示 -->  未实现
- 支持多种色深（8/16/32位）

### 4. 格式转换模块 (convert/)

**功能**：将摄像头数据格式转换为LCD显示格式

**支持的转换器**：
| 转换器 | 输入格式 | 输出格式 |
|--------|----------|----------|
| yuv2rgb | YUV422 | RGB565/RGB32 |
| mjpeg2rgb | MJPEG | RGB565/RGB32 |
| rgb2rgb | RGB565 | RGB565/RGB32 |

**核心结构体**：
- `T_VideoConvert`：格式转换器接口

### 5. 渲染模块 (render/)

**功能**：图像处理操作

**支持操作**：
- `PicZoom()`：图像缩放（最近邻插值）
- `PicMerge()`：图像合并

---

## 程序流程图

### 主程序流程

```
┌─────────────────────────────────────────────────────────────┐
│                         程序开始                             │
└─────────────────────────┬───────────────────────────────────┘
                          │
                          ▼
┌─────────────────────────────────────────────────────────────┐
│                    解析命令行参数                             │
│                   ./video2lcd /dev/videoX                    │
└─────────────────────────┬───────────────────────────────────┘
                          │
                          ▼
┌─────────────────────────────────────────────────────────────┐
│                   第一阶段：设备初始化                        │
├─────────────────────────────────────────────────────────────┤
│  1. DisplayInit()          - 初始化显示设备管理              │
│  2. SelectAndInitDefaultDispDev("fb") - 初始化Framebuffer   │
│  3. GetDispResolution()    - 获取LCD分辨率                  │
│  4. GetVideoBufForDisplay()- 获取帧缓冲区                   │
│  5. VideoInit()            - 初始化视频设备管理              │
│  6. VideoDeviceInit()      - 初始化摄像头设备                │
│  7. VideoConvertInit()     - 初始化格式转换模块              │
│  8. GetVideoConvertForFormats() - 获取合适的转换器           │
└─────────────────────────┬───────────────────────────────────┘
                          │
                          ▼
┌─────────────────────────────────────────────────────────────┐
│                   第二阶段：启动摄像头                        │
│              StartDevice() - 开始图像采集                    │
└─────────────────────────┬───────────────────────────────────┘
                          │
                          ▼
┌─────────────────────────────────────────────────────────────┐
│                   第三阶段：主循环                            │
│                                                              │
│   ┌─────────────────────────────────────────────────────┐   │
│   │  步骤1: GetFrame() - 从摄像头获取一帧图像            │   │
│   └─────────────────────────┬───────────────────────────┘   │
│                             │                               │
│                             ▼                               │
│   ┌─────────────────────────────────────────────────────┐   │
│   │  步骤2: 格式判断                                      │   │
│   │  if (摄像头格式 != LCD格式)                           │   │
│   │      Convert() - 格式转换为RGB                       │   │
│   └─────────────────────────┬───────────────────────────┘   │
│                             │                               │
│                             ▼                               │
│   ┌─────────────────────────────────────────────────────┐   │
│   │  步骤3: 尺寸判断                                      │   │
│   │  if (图像尺寸 > LCD尺寸)                              │   │
│   │      PicZoom() - 等比例缩放图像                      │   │
│   └─────────────────────────┬───────────────────────────┘   │
│                             │                               │
│                             ▼                               │
│   ┌─────────────────────────────────────────────────────┐   │
│   │  步骤4: 计算居中位置                                  │   │
│   │  iTopLeftX = (LCD宽 - 图像宽) / 2                    │   │
│   │  iTopLeftY = (LCD高 - 图像高) / 2                    │   │
│   └─────────────────────────┬───────────────────────────┘   │
│                             │                               │
│                             ▼                               │
│   ┌─────────────────────────────────────────────────────┐   │
│   │  步骤5: PicMerge() - 将图像合并到帧缓冲区            │   │
│   └─────────────────────────┬───────────────────────────┘   │
│                             │                               │
│                             ▼                               │
│   ┌─────────────────────────────────────────────────────┐   │
│   │  步骤6: FlushPixelDatasToDev() - 刷新到LCD显示       │   │
│   └─────────────────────────┬───────────────────────────┘   │
│                             │                               │
│                             ▼                               │
│   ┌─────────────────────────────────────────────────────┐   │
│   │  步骤7: PutFrame() - 归还缓冲区给摄像头              │   │
│   └─────────────────────────┬───────────────────────────┘   │
│                             │                               │
│                             └───────────┐                   │
│                                         │ 循环              │
│                                         ▼                   │
│                              回到步骤1                      │
└─────────────────────────────────────────────────────────────┘
```

---

## 模块调用关系图

```
┌─────────────────────────────────────────────────────────────────────┐
│                           main.c                                    │
│                         (主程序)                                    │
└─────────────────────────────────────────────────────────────────────┘
           │              │              │              │
           ▼              ▼              ▼              ▼
┌──────────────────┐ ┌──────────────────┐ ┌──────────────────┐ ┌──────────────────┐
│  disp_manager    │ │  video_manager   │ │ convert_manager  │ │     render       │
│  (显示管理)      │ │  (视频管理)      │ │  (转换管理)      │ │    (渲染)        │
└──────────────────┘ └──────────────────┘ └──────────────────┘ └──────────────────┘
           │              │              │              │
           ▼              ▼              ▼              ▼
┌──────────────────┐ ┌──────────────────┐ ┌──────────────────┐ ┌──────────────────┐
│      fb.c        │ │      v4l2.c      │ │   yuv2rgb.c      │ │     zoom.c       │
│  (Framebuffer)   │ │   (V4L2驱动)     │ │   mjpeg2rgb.c    │ │     merge.c      │
│                  │ │                  │ │   rgb2rgb.c      │ │                  │
│                  │ │                  │ │   color.c        │ │                  │
└──────────────────┘ └──────────────────┘ └──────────────────┘ └──────────────────┘
```

---

## 函数调用链

### 初始化阶段调用链

```
main()
├── DisplayInit()
│   └── FBInit()
│       └── RegisterDispOpr(&g_tFBOpr)
│
├── SelectAndInitDefaultDispDev("fb")
│   ├── GetDispOpr("fb")
│   ├── DeviceInit() -> FBDeviceInit()
│   │   ├── open("/dev/fb0")
│   │   ├── ioctl(FBIOGET_VSCREENINFO)
│   │   ├── ioctl(FBIOGET_FSCREENINFO)
│   │   └── mmap()
│   └── CleanScreen() -> FBCleanScreen()
│
├── GetDispResolution()
├── GetVideoBufForDisplay()
│
├── VideoInit()
│   └── V4l2Init()
│       └── RegisterVideoOpr(&g_tV4l2VideoOpr)
│
├── VideoDeviceInit()
│   └── V4l2InitDevice()
│       ├── open("/dev/videoX")
│       ├── ioctl(VIDIOC_QUERYCAP)
│       ├── ioctl(VIDIOC_ENUM_FMT)
│       ├── ioctl(VIDIOC_S_FMT)
│       ├── ioctl(VIDIOC_REQBUFS)
│       ├── ioctl(VIDIOC_QUERYBUF)
│       ├── mmap() (多个缓冲区)
│       └── ioctl(VIDIOC_QBUF) (入队)
│
├── VideoConvertInit()
│   ├── Yuv2RgbInit()
│   │   ├── initLut()
│   │   └── RegisterVideoConvert()
│   ├── Mjpeg2RgbInit()
│   │   └── RegisterVideoConvert()
│   └── Rgb2RgbInit()
│       └── RegisterVideoConvert()
│
└── GetVideoConvertForFormats()
```

### 主循环调用链

```
while(1)
├── GetFrame() -> V4l2GetFrameForStreaming()
│   ├── poll() (等待数据)
│   └── ioctl(VIDIOC_DQBUF) (出队)
│
├── Convert() (如果需要格式转换)
│   ├── Yuv2RgbConvert()
│   │   └── Pyuv422torgb565/Pyuv422torgb32()
│   ├── Mjpeg2RgbConvert()
│   │   ├── jpeg_create_decompress()
│   │   ├── jpeg_mem_src_tj()
│   │   ├── jpeg_read_header()
│   │   ├── jpeg_start_decompress()
│   │   ├── jpeg_read_scanlines()
│   │   │   └── CovertOneLine()
│   │   ├── jpeg_finish_decompress()
│   │   └── jpeg_destroy_decompress()
│   └── Rgb2RgbConvert()
│
├── PicZoom() (如果需要缩放)
│   └── 最近邻插值算法
│
├── PicMerge()
│   └── memcpy() (逐行复制)
│
├── FlushPixelDatasToDev()
│   └── ShowPage() -> FBShowPage()
│       └── memcpy() (到framebuffer)
│
└── PutFrame() -> V4l2PutFrameForStreaming()
    └── ioctl(VIDIOC_QBUF) (入队)
```

---

## 数据结构关系图

```
┌─────────────────────────────────────────────────────────────────────┐
│                        T_PixelDatas                                 │
│                     (图像像素数据)                                  │
├─────────────────────────────────────────────────────────────────────┤
│  iWidth       : 图像宽度                                           │
│  iHeight      : 图像高度                                           │
│  iBpp         : 每像素位数                                         │
│  iLineBytes   : 每行字节数                                         │
│  iTotalBytes  : 总字节数                                           │
│  *aucPixelDatas : 像素数据指针                                     │
└─────────────────────────────────────────────────────────────────────┘
                              ▲
                              │ 包含
        ┌─────────────────────┼─────────────────────┐
        │                     │                     │
┌───────┴────────┐   ┌────────┴────────┐   ┌────────┴────────┐
│   T_VideoBuf   │   │   T_VideoMem    │   │    T_DispOpr    │
│  (视频缓冲区)  │   │  (视频内存)     │   │  (显示操作)     │
├────────────────┤   ├─────────────────┤   ├─────────────────┤
│ tPixelDatas    │   │ tPixelDatas     │   │ pucDispMem      │
│ iPixelFormat   │   │ eVideoMemState  │   │ iXres, iYres    │
└────────────────┘   │ ePicState       │   │ iBpp            │
                     │ *ptNext         │   │ *ptNext         │
                     └─────────────────┘   └─────────────────┘

┌─────────────────────────────────────────────────────────────────────┐
│                        T_VideoDevice                                │
│                       (视频设备)                                    │
├─────────────────────────────────────────────────────────────────────┤
│  iFd            : 设备文件描述符                                    │
│  iPixelFormat   : 当前像素格式                                      │
│  iWidth         : 图像宽度                                         │
│  iHeight        : 图像高度                                         │
│  iVideoBufCnt   : 缓冲区数量                                       │
│  pucVideBuf[]   : 缓冲区数组(mmap映射)                             │
│  *ptOPr         : 操作接口指针                                     │
└─────────────────────────────────────────────────────────────────────┘
                              │
                              │ 指向
                              ▼
┌─────────────────────────────────────────────────────────────────────┐
│                         T_VideoOpr                                  │
│                       (视频操作接口)                                │
├─────────────────────────────────────────────────────────────────────┤
│  *name          : 驱动名称                                         │
│  InitDevice()   : 初始化设备                                        │
│  ExitDevice()   : 退出设备                                          │
│  GetFrame()     : 获取一帧                                          │
│  PutFrame()     : 归还缓冲区                                        │
│  StartDevice()  : 启动设备                                          │
│  StopDevice()   : 停止设备                                          │
│  *ptNext        : 链表指针                                          │
└─────────────────────────────────────────────────────────────────────┘
```

---

## 编译与运行

### 编译

```bash
cd video2lcd
make
```

### 运行

```bash
./video2lcd /dev/video0
```

### 依赖

- Linux Kernel (支持V4L2和Framebuffer)
- libjpeg (用于MJPEG解码)

---

## 技术要点

### 1. V4L2视频采集

- 使用内存映射（mmap）方式获取数据，效率高
- 支持多缓冲区轮转，实现流畅采集
- 自动选择设备支持的格式

### 2. 格式转换

- 使用查找表（LUT）优化YUV->RGB转换性能
- 支持多种输入格式自动适配
- MJPEG使用libjpeg库解码

### 3. 图像缩放

- 使用最近邻插值算法
- 保持原始宽高比
- 自动计算缩放尺寸

### 4. 显示优化

- 直接操作Framebuffer显存
- 计算居中显示位置
- 支持多种色深

---

## 文件说明

| 文件 | 说明 |
|------|------|
| main.c | 主程序，协调各模块工作 |
| video_manager.c | 视频设备管理，注册/选择驱动 |
| v4l2.c | V4L2驱动实现，摄像头操作 |
| disp_manager.c | 显示设备管理，VideoMem管理 |
| fb.c | Framebuffer驱动实现 |
| convert_manager.c | 格式转换管理，注册/选择转换器 |
| yuv2rgb.c | YUV422 -> RGB 转换 |
| mjpeg2rgb.c | MJPEG -> RGB 转换 |
| rgb2rgb.c | RGB565 -> RGB32 转换 |
| color.c | 颜色空间转换查找表 |
| zoom.c | 图像缩放（最近邻插值） |
| merge.c | 图像合并 |

---


