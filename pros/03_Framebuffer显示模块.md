# 03 · Framebuffer 显示模块深度拷打

> 对应源码：`display/fb.c`、`display/disp_manager.c`、`include/disp_manager.h`。

---

## Q1. 什么是 Framebuffer？它在 Linux 显示子系统里扮演什么角色？

**参考答案：**

Framebuffer（帧缓冲）是 Linux 内核提供的一个**显示设备抽象层**，把显卡/显示控制器的显存抽象成一个**设备文件 `/dev/fb0`**。应用程序可以像操作普通文件一样，通过 `open/read/write/ioctl/mmap` 来操作它，把像素数据写进显存，显示控制器就会周期性地把显存内容扫描输出到 LCD 屏幕。

它的意义：**屏蔽底层显示硬件差异**，应用不用直接操作显卡寄存器，只要往这块「内存」里写像素即可。显示控制器在后台以固定刷新率（如 60Hz）不断读取这块显存并输出到屏幕。

关键点：**往 framebuffer 写数据 = 直接写显存**，所以本项目用 mmap 把它映射到用户空间，就能高效地直接操作屏幕像素。

---

## Q2. 讲讲 Framebuffer 的初始化和操作流程

**参考答案（对应 FBDeviceInit）：**

```c
g_fd = open("/dev/fb0", O_RDWR);                          // 1. 打开设备
ioctl(g_fd, FBIOGET_VSCREENINFO, &g_tFBVar);              // 2. 获取可变信息
ioctl(g_fd, FBIOGET_FSCREENINFO, &g_tFBFix);              // 3. 获取固定信息
g_dwScreenSize = xres * yres * bits_per_pixel / 8;        // 4. 计算显存大小
g_pucFBMem = mmap(NULL, g_dwScreenSize, PROT_READ|PROT_WRITE, MAP_SHARED, g_fd, 0);  // 5. 映射
```

1. **open** `/dev/fb0`，读写方式。
2. **FBIOGET_VSCREENINFO** 获取**可变信息**（`fb_var_screeninfo`）：分辨率 xres/yres、色深 bits_per_pixel、虚拟分辨率等——这些可以被应用修改。
3. **FBIOGET_FSCREENINFO** 获取**固定信息**（`fb_fix_screeninfo`）：显存物理地址、行长度（line_length，一行占多少字节）、显存总大小等——这些由硬件决定，不可改。
4. 计算屏幕总字节数 = 宽 × 高 × 每像素字节数。
5. **mmap** 把显存映射到用户空间，得到 `g_pucFBMem` 指针，之后直接往这个指针写就是往屏幕写。用 `PROT_READ|PROT_WRITE`（要写显存），`MAP_SHARED`（和内核共享）。

---

## Q3. fb_var_screeninfo 和 fb_fix_screeninfo 有什么区别？

**参考答案：**

- **`fb_var_screeninfo`（可变信息，var = variable）**：包含**可以被应用程序修改**的显示参数——分辨率（xres/yres）、虚拟分辨率（xres_virtual/yres_virtual）、色深（bits_per_pixel）、RGB 各分量的位域偏移（red/green/blue 的 offset 和 length）、xoffset/yoffset（用于滚屏/双缓冲）等。可以用 `FBIOPUT_VSCREENINFO` 修改。
- **`fb_fix_screeninfo`（固定信息，fix = fixed）**：包含**由硬件决定、不可修改**的参数——显存物理起始地址（smem_start）、显存长度（smem_len）、**一行占用的字节数（line_length）**、加速类型等。

本项目主要用 var 里的 xres/yres/bits_per_pixel 来算屏幕参数。

**追问：为什么算地址用 line_length 而不是 xres × bpp/8？**
因为显存的一行可能有**对齐填充**（padding），实际行字节数 `line_length` 可能大于 `xres × bpp/8`。用 line_length 更准确。不过本项目代码里用的是 `xres * bpp / 8` 算 g_dwLineWidth，假设了没有填充——在多数嵌入式 LCD 上成立，但严格说应该用 fix.line_length，这是一个可讨论的细节。

---

## Q4. 一个像素在 framebuffer 显存里的地址怎么算？

**参考答案（对应 FBShowPixel）：**

```c
pucFB = g_pucFBMem + iY * g_dwLineWidth + iX * g_dwPixelWidth;
```
- `g_pucFBMem`：显存起始地址（mmap 返回）。
- `iY * g_dwLineWidth`：跳过前面 iY 行，`g_dwLineWidth` = 每行字节数（`xres * bpp / 8`）。
- `iX * g_dwPixelWidth`：在当前行内跳过 iX 个像素，`g_dwPixelWidth` = 每像素字节数（`bpp / 8`）。

framebuffer 是**行式线性存储**：从左上角开始，一行一行、从左到右连续排列。所以 (x,y) 像素的字节偏移 = y × 行字节数 + x × 每像素字节数。

**追问：这里有个边界判断的 bug 你能看出来吗？**
```c
if((iX > g_tFBVar.xres) || (iY > g_tFBVar.yres))  // 用了 > 
```
应该是 `>=`。因为合法坐标是 0 ~ xres-1，当 iX == xres 时已经越界了，但 `>` 判断不会拦截，会导致写到显存外。这是复盘发现的一个 off-by-one 隐患（虽然本项目主流程没直接调 ShowPixel，走的是 ShowPage 整页拷贝）。

---

## Q5. framebuffer 支持哪几种色深（bpp）？你是怎么适配的？

**参考答案：**

本项目在画点（FBShowPixel）和清屏（FBCleanScreen）里用 `switch(bits_per_pixel)` 适配三种色深：

- **8 bpp**：每像素 1 字节，通常是调色板/灰度模式，直接写一个字节。
- **16 bpp**：RGB565 格式，每像素 2 字节。要把传入的 32 位颜色（0x00RRGGBB）转成 16 位：
  ```c
  iRed   = (dwColor >> (16+3)) & 0x1f;   // 8位R取高5位
  iGreen = (dwColor >> (8+2))  & 0x3f;   // 8位G取高6位
  iBlue  = (dwColor >> 3)      & 0x1f;   // 8位B取高5位
  wColor16bpp = (iRed << 11) | (iGreen << 5) | iBlue;
  ```
- **32 bpp**：每像素 4 字节（0x00RRGGBB 或带 Alpha），直接写 32 位。

**RGB565 的含义**：16 位里 R 占 5 位、G 占 6 位、B 占 5 位。绿色多 1 位是因为**人眼对绿色最敏感**。从 8 位分量转到 5/6 位要右移丢掉低位（R/B 右移 3，G 右移 2）。

**追问：32 位色和 24 位色的区别？**
24 位是真彩色 RGB888（每像素 3 字节），32 位通常是 RGB888 + 8 位 padding/Alpha（每像素 4 字节）。32 位因为按 4 字节对齐，CPU 访问更快，是更常见的 framebuffer 格式。本项目 LCD 是 32bpp（从 GetVideoBufForDisplay 里 bpp==32 走 RGB32 可推断）。

---

## Q6. ShowPage 是怎么把一帧数据显示到屏幕的？

**参考答案（对应 FBShowPage）：**

```c
static int FBShowPage(PT_PixelDatas ptPixelDatas){
    if(g_tFBOpr.pucDispMem != ptPixelDatas->aucPixelDatas){
        memcpy(g_tFBOpr.pucDispMem, ptPixelDatas->aucPixelDatas, ptPixelDatas->iTotalBytes);
    }
    return 0;
}
```
- 判断要显示的数据是不是**已经在显存里**了（`pucDispMem` 是 mmap 的显存地址）。如果数据指针不等于显存地址，说明数据在别处（比如某个 VideoMem），需要 `memcpy` 拷贝到显存。
- 如果**已经相等**（数据就在显存），就跳过拷贝，避免自己拷自己的无谓开销。

本项目主流程里，`tFrameBuf.aucPixelDatas` 通过 `GetVideoBufForDisplay` **直接指向了 framebuffer 显存**，PicMerge 是直接把图像合并写进显存的，所以 ShowPage 时两者相等，**这次 memcpy 被跳过**——这是一个巧妙的优化，省掉了一次全屏拷贝。

---

## Q7. 讲讲 GetVideoBufForDisplay 的设计巧思

**参考答案：**

```c
int GetVideoBufForDisplay(PT_VideoBuf ptFrameBuf){
    ptFrameBuf->iPixelFormat = (iBpp==16) ? RGB565 : (iBpp==32) ? RGB32 : 0;
    ptFrameBuf->tPixelDatas.iWidth/iHeight/iBpp/iLineBytes = ...;   // 用 LCD 参数
    ptFrameBuf->tPixelDatas.aucPixelDatas = g_ptDefaultDispOpr->pucDispMem;  // 直接指向显存!
}
```

关键点：`tFrameBuf` 这个「帧缓冲区」的像素数据指针**直接指向 framebuffer 的 mmap 显存**，而不是另外 malloc 一块。

好处：主循环里 PicMerge 把图像直接合并写进 `tFrameBuf`（也就是显存），FlushPixelDatasToDev → ShowPage 时发现数据已在显存，**跳过 memcpy**。相当于「就地渲染」，减少一次全屏内存拷贝，提升性能。

它还会根据 LCD 的 bpp 自动设置目标像素格式（16bpp→RGB565，32bpp→RGB32），这个格式就是转换器的输出目标格式，保证转换结果能直接显示。

---

## Q8. disp_manager 里的 VideoMem 机制是干什么的？主流程用到了吗？

**参考答案（诚实回答，体现理解）：**

`VideoMem` 是一套**多页显示缓冲机制**，设计目的是**加速显示、支持双缓冲/多缓冲**：
- 先在内存里的 VideoMem 页上把图像画好（后台准备），再一次性拷贝到显存，避免「边画边显示」造成的闪烁/撕裂。
- 支持多个 VideoMem 页，通过状态机管理：
  - `E_VideoMemState`：VMS_FREE（空闲）/ VMS_USED_FOR_PREPARE（准备中）/ VMS_USED_FOR_CUR（当前显示）。
  - `E_PicState`：PS_BLANK（空白）/ PS_GENERATING（生成中）/ PS_GENERATED（已生成）。
- `GetVideoMem(iID, bCur)` 有 4 级分配策略：先找 ID 匹配的空闲页 → 找空闲且空白的页 → 找任意空闲页 → bCur=1 时强行拿一个页。

**但是**：本项目主流程（main.c）**并没有使用 VideoMem**，而是用 `GetVideoBufForDisplay` 直接操作显存。VideoMem 相关函数（AllocVideoMem/GetVideoMem/PutVideoMem/ClearVideoMem）是预留的，注释里写了「未来配合 lvgl 开发」。所以面试时要诚实说明：**这是框架预留能力，当前场景（单一全屏相机画面）用直接显存操作更简单高效，多缓冲机制暂未启用。**

**追问：什么情况下需要 VideoMem？**
当界面复杂（多个图层、UI 控件、需要局部刷新）或需要避免撕裂时，多缓冲 + 后台渲染就有价值了。相机全屏刷新这种场景，直接写显存反而更省内存和拷贝。

---

## Q9. disp_manager 的注册机制和 video_manager 一样吗？

**参考答案：**

一样的套路，也是**函数指针结构体链表**：
- `RegisterDispOpr()`：尾插法注册显示设备到 `g_ptDispOprHead` 链表。
- `GetDispOpr(name)`：按名字查找。
- `SelectAndInitDefaultDispDev("fb")`：找到 fb 驱动，设为默认 `g_ptDefaultDispOpr`，调 `DeviceInit()` 初始化 + `CleanScreen(0)` 清屏为黑。
- `DisplayInit()` → `FBInit()` → `RegisterDispOpr(&g_tFBOpr)`。

三个管理器（video/display/convert）用的是**完全一致的注册-查找模式**，体现了架构的统一性。这也是本项目的一个设计特色：**用同一套 C 语言面向对象范式贯穿所有模块**。

---

## Q10. FlushPixelDatasToDev 做了什么？为什么单独封装一个函数？

**参考答案：**

```c
void FlushPixelDatasToDev(PT_PixelDatas ptPixelDatas){
    g_ptDefaultDispOpr->ShowPage(ptPixelDatas);
}
```
它就是调用当前默认显示设备的 `ShowPage` 方法，把像素数据输出到屏幕。

**为什么封装**：
1. **解耦**：main.c 不需要知道具体是 fb 还是别的显示设备，只调 `FlushPixelDatasToDev`，内部通过函数指针分发到具体实现。
2. **统一接口**：换了显示设备（比如 HDMI），main 代码不用改，只要切换默认设备即可。
3. **语义清晰**：函数名表达了「刷新到设备」的意图。

这是典型的**门面/代理模式**：管理层对上提供简洁统一的接口，对下通过函数指针分发。

---

## Q11. 直接往显存写会不会有画面撕裂？怎么解决？

**参考答案（高频追问）：**

**会有可能。** 画面撕裂（tearing）的原因：显示控制器以固定刷新率（如 60Hz）**从上到下扫描**显存输出到屏幕，如果应用在扫描过程中修改显存（写入新的一帧），那么屏幕上半部分显示的是新帧、下半部分是旧帧，交界处出现「撕裂线」。

本项目直接写显存，理论上存在这个问题。解决方案：
1. **双缓冲 + 垂直同步（VSync）**：在后台缓冲区（back buffer）画好完整一帧，等屏幕扫描到**垂直消隐期（VSync 信号）**时，再快速切换/拷贝到前台缓冲区。切换发生在扫描间隙，就不会撕裂。这正是 `VideoMem` 机制和 framebuffer 的 `yoffset`（配合 FBIOPAN_DISPLAY）能实现的。
2. **本项目现状**：因为是相机实时画面、刷新快，单帧撕裂不明显，且当前用的是「就地写显存」的简单方案。要彻底解决需要启用双缓冲 + VSync 同步。

---

## Q12. mmap 显存和 malloc 一块内存有什么区别？

**参考答案：**

| | mmap 显存 | malloc 普通内存 |
|---|---|---|
| 内存来源 | 映射的是**设备的物理显存**（framebuffer） | 进程堆内存（物理内存页） |
| 效果 | 写它 = 直接写显存 = 屏幕立即变化 | 写它只是改内存，屏幕无反应 |
| 底层 | 建立用户虚拟地址 → 设备物理地址的映射 | 分配匿名内存页 |
| 用途 | 和硬件交互（显示、DMA 缓冲） | 普通数据存储 |

关键：mmap 显存建立的是**用户空间虚拟地址到设备物理显存的映射**，所以往这个指针写数据，硬件（显示控制器）能直接读到并显示。malloc 的内存硬件是看不到的。

本项目两处 mmap：一处映射摄像头采集缓冲区（V4L2），一处映射 framebuffer 显存，都是为了让用户态直接访问硬件相关的物理内存，实现零拷贝。

---

## Q13. FBCleanScreen 清屏是怎么实现的？为什么不同色深处理不一样？

**参考答案：**

清屏就是把整个显存填充成某个颜色：
- **8 bpp**：直接 `memset(g_pucFBMem, dwBackColor, g_dwScreenSize)`，因为一个字节就是一个像素。
- **16 bpp**：不能用 memset（memset 按字节填充，而一个像素是 2 字节），要先把 32 位颜色转成 16 位 RGB565，然后循环 `*pwFB16bpp++ = wColor16bpp`，每次写 2 字节。
- **32 bpp**：循环 `*pdwFB32bpp++ = dwBackColor`，每次写 4 字节。

**为什么 memset 只对 8bpp 有效**：memset 是按**字节**填充相同值。16/32bpp 一个像素是多字节，如果直接 memset 一个颜色值，只有当该颜色的每个字节都相同时才对（比如全 0 黑色可以），否则会出错。所以多字节色深要按像素单位循环写。

---

## 附：Framebuffer 关键概念速查

| 概念 | 含义 |
|------|------|
| /dev/fb0 | framebuffer 设备节点 |
| FBIOGET_VSCREENINFO | 获取可变显示信息（分辨率、色深、位域） |
| FBIOGET_FSCREENINFO | 获取固定显示信息（显存地址、行长度） |
| FBIOPUT_VSCREENINFO | 设置显示参数 |
| FBIOPAN_DISPLAY | 平移显示窗口（配合 yoffset 做双缓冲切换） |
| bits_per_pixel (bpp) | 每像素位数：8/16/24/32 |
| line_length | 一行的字节数（可能含对齐填充） |
| smem_start | 显存物理起始地址 |
| RGB565 | 16 位色：R5 G6 B5 |
