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
rgb565 就是每个颜色取高位  高5位红  6位绿 5位蓝
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

##这是典型的**门面/代理模式**：管理层对上提供简洁统一的接口，对下通过函数指针分发。

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
malloc的虚拟内存映射道的是内存条上的普通内存
mmap的是设备内存mmio
| | mmap 显存 | malloc 普通内存 |
|---|---|---|
| 内存来源 | 映射的是**设备的物理显存**（framebuffer） | 进程堆内存（物理内存页） |
| 效果 | 写它 = 直接写显存 = 屏幕立即变化 | 写它只是改内存，屏幕无反应 |
| 底层 | 建立用户虚拟地址 → 设备物理地址的映射 | 分配匿名内存页 |
| 用途 | 和硬件交互（显示、DMA 缓冲） | 普通数据存储 |

关键：mmap 显存建立的是**用户空间虚拟地址到设备物理显存的映射**，所以往这个指针写数据，硬件（显示控制器）能直接读到并显示。malloc 的内存硬件是看不到的。

本项目两处 mmap：一处映射摄像头采集缓冲区（V4L2），一处映射 framebuffer 显存，都是为了让用户态直接访问硬件相关的物理内存，实现零拷贝。

---

## Q13. mmap 的几个参数分别是什么含义？（结合本项目两处调用）

**参考答案：**

函数原型：
```c
void *mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset);
```

六个参数逐个讲：

| 参数 | 含义 | 本项目怎么传 |
|------|------|-------------|
| `addr` | 建议的映射起始地址，通常传 `NULL`/`0` 让内核自己选一个合适地址 | 都传 `NULL`/`0`，由内核决定 |
| `length` | 映射的字节长度 | fb 传 `g_dwScreenSize`（整屏大小）；v4l2 传 `tV4l2Buf.length`（单个采集缓冲区大小） |
| `prot` | 映射区的**访问权限**（protection） | fb 传 `PROT_READ\|PROT_WRITE`（要往显存写）；v4l2 传 `PROT_READ`（数据由内核/DMA写入，用户只读） |
| `flags` | 映射**类型/共享方式** | 都用 `MAP_SHARED`（与内核共享同一物理内存，是零拷贝的关键） |
| `fd` | 被映射对象的文件描述符 | fb 传 `/dev/fb0` 的 `g_fd`；v4l2 传摄像头设备的 `iFd` |
| `offset` | 从文件的哪个偏移开始映射 | fb 传 `0`（显存从头映射）；v4l2 传 `tV4l2Buf.m.offset`（每个采集缓冲区偏移不同，由 QUERYBUF 返回） |

**几个关键参数展开：**

- **`prot`（权限）**：常用 `PROT_READ`（可读）、`PROT_WRITE`（可写）、`PROT_EXEC`（可执行）、`PROT_NONE`（不可访问）。权限不能超出文件的打开方式——比如 fd 是只读打开的，就不能 `PROT_WRITE`。fb 需要写显存所以带 `PROT_WRITE`；v4l2 采集缓冲区是内核往里写、用户只读，用 `PROT_READ` 即可。

- **`flags`（共享方式）**：最核心的两个值——
  - `MAP_SHARED`：映射区**与其他进程/内核共享同一块物理内存**，对它的修改会同步反映到设备/文件。这是实现零拷贝的前提。
  - `MAP_PRIVATE`：私有映射，**写时复制（COW）**，修改不会同步给别人。若用它，v4l2 采集和 fb 显存都拿不到实时数据。

- **`offset`（偏移）**：要求按页对齐（通常 4KB 的整数倍）。fb 显存从 0 开始整块映射；v4l2 每个缓冲区内核给的 `m.offset` 不同，所以循环里每次映射到不同的物理块。

**返回值**：成功返回映射后的用户态首地址；失败返回 `MAP_FAILED`（即 `(void*)-1`），**不是返回 NULL**，所以判断失败要用 `== MAP_FAILED`（本项目 v4l2.c、fb.c 都是这么判断的）。

**两处调用对比记忆：**
```c
// fb.c：映射显存——要写，所以 PROT_READ|PROT_WRITE，offset=0
g_pucFBMem = mmap(NULL, g_dwScreenSize, PROT_READ|PROT_WRITE, MAP_SHARED, g_fd, 0);

// v4l2.c：映射采集缓冲区——只读，PROT_READ，offset 每块不同
pucVideBuf[i] = mmap(0, tV4l2Buf.length, PROT_READ, MAP_SHARED, iFd, tV4l2Buf.m.offset);
```
一句话：**fb 是「读写显存」，v4l2 是「只读采集缓冲」，两者都靠 `MAP_SHARED` 和硬件共享物理内存，实现零拷贝。**

---

## Q14. FBCleanScreen 清屏是怎么实现的？为什么不同色深处理不一样？

**参考答案（对应 FBCleanScreen）：**

清屏的本质是「把整块显存的每个像素都填成同一个颜色」。先看源码骨架：

```c
static int FBCleanScreen(unsigned int dwBackColor)
{
    unsigned char  *pucFB      = g_pucFBMem;
    unsigned short *pwFB16bpp  = (unsigned short *)pucFB;   // 按 2 字节访问
    unsigned int   *pdwFB32bpp = (unsigned int *)pucFB;     // 按 4 字节访问
    int i = 0;

    switch (g_tFBVar.bits_per_pixel)
    {
      memset会只截断到最低1字节 所以color是32位无所谓
        case 8:   memset(g_pucFBMem, dwBackColor, g_dwScreenSize); break;
        case 16:
            iRed   = (dwBackColor >> (16+3)) & 0x1f;
            iGreen = (dwBackColor >> (8+2))  & 0x3f;
            iBlue  = (dwBackColor >> 3)      & 0x1f;
            wColor16bpp = (iRed << 11) | (iGreen << 5) | iBlue;
            while (i < g_dwScreenSize) { *pwFB16bpp++ = wColor16bpp; i += 2; }
            break;
        case 32:
            while (i < g_dwScreenSize) { *pdwFB32bpp++ = dwBackColor; i += 4; }
            break;
    }
    return 0;
}
```

三种色深写法不同，根源是「一个像素占几个字节」：

| 色深 | 每像素字节 | 写法 | 计数器步进 |
|------|-----------|------|-----------|
| 8bpp | 1 字节 | `memset(mem, color, size)` | memset 内部按字节，一次搞定 |
| 16bpp | 2 字节 | 循环 `*pwFB16bpp++ = wColor16bpp` | `i += 2` |
| 32bpp | 4 字节 | 循环 `*pdwFB32bpp++ = dwBackColor` | `i += 4` |

### 一、循环计数逻辑：靠 `i += 每像素字节数` 控制

整屏总字节数在初始化时算好：

```c
g_dwScreenSize = xres * yres * bits_per_pixel / 8;   // 整屏总字节数
```

循环里的 `i` 是「已经写了多少字节」的计数器：

```c
while (i < g_dwScreenSize)   // i 从 0 累加到整屏总字节数
{
    *pwFB16bpp = wColor16bpp; // 写一个像素（2 字节）
    pwFB16bpp++;              // 指针移到下一个像素
    i += 2;                   // ★已写字节数 +2（因为一个像素 2 字节）★
}
```

- 16bpp 一个像素 2 字节，每写一个 `i += 2`；32bpp 一个像素 4 字节，`i += 4`。
- 循环条件 `i < g_dwScreenSize`：写满整屏总字节数就停。
- **循环次数 = `g_dwScreenSize / 每像素字节` = 总像素数（xres × yres）**。
- 指针 `pwFB16bpp++` 是 `unsigned short*`（2 字节类型），`++` 正好跳过 2 字节到下一个像素；`pdwFB32bpp` 是 `unsigned int*`（4 字节类型），`++` 跳过 4 字节。**指针类型天然决定了步进对齐**，不用手动加偏移。

### 二、为什么 16/32bpp 不能用 memset

`memset` 是**按字节**填充相同值：

- **8bpp**：一个像素就是一个字节，每字节填同一个颜色值 → 正确。
- **16bpp**：一个像素占 2 字节。若 `memset(mem, 0x33, size)`，会把每个字节都填成 `0x33`，两字节合成 `0x3333`，这**不是想要的颜色**。只有当目标颜色的高低字节恰好相同（如黑色 0x0000）才碰巧正确，所以必须先把颜色算成 16 位 `wColor16bpp`，再按 2 字节为单位循环写。
- **32bpp**：一个像素占 4 字节，memset 同样会把颜色值按字节拆散，得按 4 字节循环写。

一句话：**memset 的粒度是字节，而 16/32bpp 的像素粒度是多字节，两者不匹配**，所以多字节色深要按像素单位循环写。

### 三、16bpp 的 RGB565 颜色位运算推导

传入的 `dwBackColor` 是 32 位 `0x00RRGGBB`（R/G/B 各占 8 位），要压缩成 16 位 RGB565（R 5 位、G 6 位、B 5 位）：

```c
iRed   = (dwBackColor >> (16+3)) & 0x1f;   // 取红
iGreen = (dwBackColor >> (8+2))  & 0x3f;   // 取绿
iBlue  = (dwBackColor >> 3)      & 0x1f;   // 取蓝
wColor16bpp = (iRed << 11) | (iGreen << 5) | iBlue;
```

**分量提取（右移丢低位 + 掩码）：**

- **红色** `>> (16+3)`：`0x00RRGGBB` 里 R 在 bit16~23。8 位压成 5 位要丢低 3 位（`>>3`），再移到最低位（`>>16`），合起来 `>>19`；`& 0x1f`（二进制 11111）取低 5 位。
- **绿色** `>> (8+2)`：G 在 bit8~15。8 位压成 6 位丢低 2 位（`>>2`），再移到最低（`>>8`），合起来 `>>10`；`& 0x3f`（111111）取低 6 位。
- **蓝色** `>> 3`：B 在 bit0~7 本就在低位。8 位压成 5 位丢低 3 位（`>>3`）；`& 0x1f` 取低 5 位。

**拼装（左移到各自位置再按位或）：**

16 位布局（高→低）为 `RRRRR GGGGGG BBBBB`：

- 红放最高 5 位 → `iRed << 11`
- 绿放中间 6 位 → `iGreen << 5`
- 蓝放最低 5 位 → `iBlue`（不移）
- 用 `|` 把三段拼成一个 16 位颜色值

绿色比红蓝多留 1 位，是因为**人眼对绿色最敏感**，多一位精度视觉更自然。

**追问：从 8 位分量转 5/6 位为什么要右移？**
右移就是丢掉低位、只保留高位，等价于把 0~255 的范围压缩到 0~31（5 位）或 0~63（6 位）。R/B 右移 3 位（256→32 级），G 右移 2 位（256→64 级），这是有损压缩，颜色会有轻微失真，属于 RGB565 格式本身的取舍。

---

## Q15. 往显存写数据硬件为什么能看到？MMIO 是什么？现在还用得广吗？

**参考答案（高频追问）：**

### 一、什么是 MMIO

MMIO（Memory-Mapped I/O，内存映射 I/O）是一种**把外设寄存器/设备内存映射到 CPU 物理地址空间**的机制。映射后，外设寄存器看起来就像一段普通内存，CPU 用普通的 load/store 指令（C 里就是指针解引用）就能读写硬件，不需要专门的 I/O 指令。

它的老对手 PIO（Port I/O，端口 I/O，x86 的 `in`/`out` 指令）现在基本被淘汰，只剩 x86 上少量历史遗留（如键盘控制器 0x60 端口）。

### 二、为什么往显存写硬件能看到

显示控制器（硬件）会**周期性地扫描 framebuffer 那块物理显存**并输出到屏幕。`mmap("/dev/fb0")` 把用户态虚拟地址映射到了这块**硬件会访问的物理显存（MMIO 区域）**上，所以：

```
用户写 *pdwFB32bpp = color
  → MMU 翻译成 framebuffer 物理地址
  → 显示控制器扫描到这块物理内存
  → 屏幕立即变化
```

对比 malloc：malloc 的虚拟地址映射到的是**内存条上的普通匿名页（anonymous page）**，硬件根本不去读它，所以写它屏幕无反应。（这也是 Q12 那张表想表达的核心：区别不在“虚拟 vs 物理”，而在“虚拟地址映射到哪块物理内存 + 硬件能否访问”。）

### 三、MMIO 和本项目的关系

从内核驱动到用户态，MMIO 贯穿了整条显示/采集链路：

- **framebuffer 显存**：`mmap("/dev/fb0")` 映射的就是显示控制器的一段 MMIO 物理内存。
- **V4L2 采集缓冲区**：摄像头/DMA 控制器通过 MMIO + DMA 把数据写进那块物理内存，用户再 mmap 出来读，实现零拷贝。
- **驱动层**：fb/V4L2 驱动里操作硬件寄存器（设分辨率、启动 DMA）用的是内核的 `readl()/writel()`，它们内部带内存屏障，保证访问顺序。

### 四、MMIO 现在还用得广吗？非常广

MMIO 是当今硬件访问的**事实标准**，尤其在嵌入式领域：

| 场景 | 说明 |
|------|------|
| ARM/RISC-V 等 SoC | GPIO、UART、I2C、SPI、定时器、DMA 控制器寄存器几乎全是 MMIO |
| GPU / 显卡 | 显存、寄存器、命令队列都通过 MMIO 映射访问 |
| PCIe 设备 | BAR 映射的设备内存/寄存器全是 MMIO |
| NVMe/SSD | 寄存器和队列都走 MMIO（PCIe BAR） |
| 中断控制器 | ARM GIC 等也是 MMIO |

为什么成主流：**统一编址**（用普通指令访问）、**地址空间大**（PIO 只有 64KB，装不下现代设备）、**架构无关**（ARM/RISC-V 根本没有 PIO 指令）、**可被 MMU 保护**。

### 五、追问：MMIO 内存能当普通内存用吗？

不能随便用。MMIO 区域通常要设成**不可缓存（uncached / device memory）**，因为：
- 寄存器值可能被硬件随时改变，若 CPU 缓存了旧值就读不到真实状态；
- 写操作若停在缓存里没真正到达设备，硬件就收不到命令。

所以内核访问 MMIO 要用带**内存屏障**的 `readl/writel`，而不是裸指针（裸指针可能被编译器优化掉或被 CPU 乱序）。用户态 mmap 显存时，内核已经帮你把缓存属性设好了，所以直接解引用是安全的。

一句话：**MMIO 不仅没被淘汰，反而是嵌入式、GPU、PCIe、DMA 等现代硬件访问的事实标准**，本项目的显示和采集底层全都依赖它。

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
| MMIO | 内存映射 I/O：把设备寄存器/显存映射到物理地址空间，用普通指令访问硬件 |
| RGB565 | 16 位色：R5 G6 B5 |
