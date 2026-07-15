# convert 目录剖析 —— 视频格式转换模块

## 一、目录概览

```
convert/
├── convert_manager.c   # 转换器管理器：注册、查找、调度
├── rgb2rgb.c           # RGB565 → RGB565/RGB32 转换
├── yuv2rgb.c           # YUV422 → RGB565/RGB32 转换
├── mjpeg2rgb.c         # MJPEG → RGB565/RGB32 转换（依赖libjpeg）
├── color.c             # 颜色转换查找表（LUT）实现
├── color.h             # 颜色转换宏定义、结构体、函数声明
├── jpeglib.h           # [第三方] libjpeg 库头文件（不用看）
├── jdatasrc-tj.c       # [第三方] libjpeg 内存数据源实现
├── jerror.h            # [第三方] libjpeg 错误处理头文件
├── jinclude.h          # [第三方] libjpeg 包含头文件
└── Makefile            # 编译脚本
```

> **注意**：`jpeglib.h`、`jdatasrc-tj.c`、`jerror.h`、`jinclude.h` 均为第三方 libjpeg 库的文件，视为黑盒，无需深入研究其内部实现。

---

## 二、核心数据结构

### 2.1 像素数据结构 `T_PixelDatas`（定义在 `pic_operation.h`）

```c
typedef struct PixelDatas {
    int iWidth;          // 图像宽度：一行有多少个像素
    int iHeight;         // 图像高度：一列有多少个像素
    int iBpp;            // 每像素位数（bits per pixel）：8/16/24/32
    int iLineBytes;      // 每行字节数 = iWidth * iBpp / 8
    int iTotalBytes;     // 图像总字节数 = iLineBytes * iHeight
    unsigned char *aucPixelDatas;  // 像素数据存储缓冲区指针
} T_PixelDatas, *PT_PixelDatas;
```

`aucPixelDatas` 是一个通用的像素数据指针，在不同阶段指向不同的内存区域：
- **采集时**：指向 V4L2 mmap 映射的摄像头缓冲区
- **转换时**：指向 `malloc` 分配的临时缓冲区
- **显示时**：指向 LCD framebuffer 显存

### 2.2 转换器接口 `T_VideoConvert`（定义在 `convert_manager.h`）

```c
typedef struct VideoConvert {
    char *name;                // 转换器名称
    int (*isSupport)(int, int);  // 判断是否支持该格式转换
    int (*Convert)(PT_VideoBuf, PT_VideoBuf);  // 执行转换
    int (*ConvertExit)(PT_VideoBuf);  // 释放转换缓冲区
    struct VideoConvert *ptNext;  // 链表指针
} T_VideoConvert, *PT_VideoConvert;
```

这是一个典型的**策略模式**接口：所有转换器实现相同的接口，管理器通过链表统一调度。

---

## 三、转换器管理器 `convert_manager.c`

### 3.1 架构

管理器通过**链表**管理所有注册的转换器：

```
g_ptVideoConvertHead → [yuv2rgb] → [mjpeg2rgb] → [rgb2rgb] → NULL
```

### 3.2 核心 API

| 函数 | 功能 |
|------|------|
| `RegisterVideoConvert()` | 将转换器挂入链表尾部 |
| `GetVideoConvert()` | 按名称查找转换器 |
| `GetVideoConvertForFormats()` | 按输入/输出像素格式自动匹配合适的转换器 |
| `VideoConvertInit()` | 依次调用所有转换器的初始化函数 |

### 3.3 调度流程

```
主程序调用 GetVideoConvertForFormats(输入格式, 输出格式)
    │
    ▼
遍历链表，调用每个转换器的 isSupport() 判断
    │
    ├── 匹配成功 → 返回该转换器指针 → 调用 Convert()
    └── 无匹配   → 返回 NULL
```

> **设计亮点**：传递的是整型像素格式标识符（FourCC 编码），而非完整结构体，减少了栈空间占用和结构体解引用开销。

---

## 四、三个转换器详解

### 4.1 RGB565 → RGB565/RGB32（`rgb2rgb.c`）

**复杂度**：⭐ 最简单

**支持的转换路径**：
- RGB565 → RGB565：直接 `memcpy`，无需计算
- RGB565 → RGB32：逐像素提取 RGB 分量并扩展

**转换原理**：

```
RGB565 (16位):  [R4 R3 R2 R1 R0] [G5 G4 G3 G2 G1 G0] [B4 B3 B2 B1 B0]
                  高5位               中6位                低5位

RGB565 → RGB565：直接内存拷贝
RGB565 → RGB32：
  R8 = R5 << 3    (5位扩展为8位)
  G8 = G6 << 2    (6位扩展为8位)
  B8 = B5 << 3    (5位扩展为8位)
  输出 = 0x00RRGGBB (32位)
```

**指针类型选择**：
- 输入指针用 `unsigned short *`（16位 = 1个RGB565像素）
- 输出指针用 `unsigned int *`（32位 = 1个RGB32像素）
- 这样 `指针++` 自动跳过正确字节数，代码简洁高效

---

### 4.2 YUV422 → RGB565/RGB32（`yuv2rgb.c`）

**复杂度**：⭐⭐⭐ 中等（涉及颜色空间转换）

**YUV422 数据格式**：

每 4 字节表示 2 个像素，U/V 色度分量由两个像素共享：

```
内存: [Y0] [U] [Y1] [V]  [Y2] [U] [Y3] [V] ...
       ↑   共享  ↑   共享
     像素0      像素1
```

**转换公式**（ITU-R BT.601）：

```
R = Y + 1.402 × (V - 128)
G = Y - 0.714 × (U - 128) - 0.344 × (V - 128)
B = Y + 1.772 × (U - 128)
```

**转换流程**：

```
每次循环处理 4 字节 → 产出 2 个像素：

① 取 4 字节: Y0=buff[0], U=buff[1], Y1=buff[2], V=buff[3]
② 像素0: 用 (Y0, U, V) 算 RGB → 打包为目标格式 → 写入输出
③ 像素1: 用 (Y1, U, V) 算 RGB → 打包为目标格式 → 写入输出
   （U、V 共享，只有 Y 不同）
④ buff += 4，继续下一对像素
```

**性能优化 — 查找表（LUT）**：

为避免嵌入式系统上的浮点运算，`color.c` 中的 `initLut()` 预计算了所有可能的转换结果：

```c
// 系数放大1000倍转为整数
#define CoefRv 1402    // 1.402 × 1000

// 预计算查找表（每张表256项）
LutRv[i] = (i - 128) * CoefRv / 1000;  // V对R的贡献
LutBu[i] = (i - 128) * CoefBu / 1000;  // U对B的贡献
LutGu[i] = (128 - i) * CoefGu / 1000;  // U对G的扣减
LutGv[i] = (128 - i) * CoefGv / 1000;  // V对G的扣减
```

运行时变为纯整数查表 + 加法：

```c
// 原始公式：R = Y + 1.402 × (V - 128)  ← 浮点乘法，慢！
// 查表方式：
R = Y + LutRv[V]   // 一次查表 + 一次加法，快！
```

**代价**：约 10KB 内存（10张表 × 256项 × 4字节），换来数十倍的速度提升。典型的**空间换时间**。

**安全规范**：所有像素运算结果通过 `CLIP` 宏限幅至 0~255：

```c
#define CLIP(color) (unsigned char)(((color)>0xFF)?0xff:(((color)<0)?0:(color)))
```

---

### 4.3 MJPEG → RGB565/RGB32（`mjpeg2rgb.c`）

**复杂度**：⭐⭐ 中等（依赖第三方库，但集成逻辑清晰）

**MJPEG 格式**：每一帧数据都是一个完整的 JPEG 文件。

**对 libjpeg 的使用视为黑盒**，调用流程如下：

```
① 创建解压对象      jpeg_create_decompress()
② 设置内存数据源    jpeg_mem_src_tj()  ← 把摄像头拿到的 MJPEG 数据喂进去
③ 读取 JPEG 头部    jpeg_read_header()  ← 获取宽高信息
④ 开始解压          jpeg_start_decompress()
⑤ 逐行读取 RGB 数据 jpeg_read_scanlines() ← 每次输出一行 24位 RGB 数据
⑥ 格式转换          CovertOneLine()  ← 将 24位 RGB 转为 RGB565 或 RGB32
⑦ 完成解压          jpeg_finish_decompress()
⑧ 销毁对象          jpeg_destroy_decompress()
```

**`CovertOneLine` 函数**：将 libjpeg 输出的 24 位 RGB 数据转换为目标格式：

```
输入: 24位 RGB (每像素3字节: R, G, B)
  │
  ├──→ RGB32: color = (R << 16) | (G << 8) | B，用 unsigned int * 一次写4字节
  │
  └──→ RGB565: R>>3, G>>2, B>>3 压缩为16位，用 unsigned short * 一次写2字节
```

**错误处理**：使用 `setjmp/longjmp` 机制实现错误恢复，避免 JPEG 解码失败导致程序崩溃。

---

## 五、颜色工具（`color.h` / `color.c`）

### 5.1 关键宏定义

| 宏 | 用途 | 方向 |
|---|------|------|
| `YfromRGB(r,g,b)` | RGB → 亮度 Y | 颜色空间转换 |
| `UfromRGB(r,g,b)` | RGB → 色度 U | 颜色空间转换 |
| `VfromRGB(r,g,b)` | RGB → 色度 V | 颜色空间转换 |
| `PACKRGB16(r,g,b)` | 8位 RGB → 16位 RGB565 | 格式打包 |
| `UNPACK16(pixel,r,g,b)` | 16位 RGB565 → 8位 RGB | 格式解包 |
| `CLIP(color)` | 限幅至 0~255 | 安全保护 |

### 5.2 整数化技巧

所有公式均使用**系数放大 + 位移**代替浮点运算：

```c
// 原始：Y = 0.299*R + 0.587*G + 0.114*B
// 整数化：系数×256取整，再>>8（等价于÷256）
#define YfromRGB(r,g,b) CLIP((77*(r)+150*(g)+29*(b))>>8)
```

### 5.3 RGB565 位布局

```
15 14 13 12 11 | 10  9  8  7  6  5 |  4  3  2  1  0
   B4 B3 B2 B1 B0 | G5 G4 G3 G2 G1 G0 | R4 R3 R2 R1 R0
   ← 5位蓝色 →    ←  6位绿色  →      ←  5位红色  →
```

> **注意字节序**：`PACKRGB16` 使用 BGR 排列（适配 LCD framebuffer），而 `rgb2rgb.c` 中使用 RGB 排列。不同硬件模块可能采用不同排列，需适配。

---

## 六、设计模式总结

### 6.1 策略模式（Strategy Pattern）

```
T_VideoConvert 接口
    ├── rgb2rgb    （RGB565 → RGB565/RGB32）
    ├── yuv2rgb    （YUV422 → RGB565/RGB32）
    └── mjpeg2rgb  （MJPEG → RGB565/RGB32）
```

所有转换器实现统一接口（`isSupport`、`Convert`、`ConvertExit`），管理器通过链表遍历自动匹配。新增转换器只需：
1. 实现 `isSupport` / `Convert` / `ConvertExit` 三个函数
2. 定义 `T_VideoConvert` 结构体
3. 在 `VideoConvertInit()` 中调用注册函数

### 6.2 统一数据载体

整个转换流水线通过 `T_PixelDatas` 统一承载像素数据，`aucPixelDatas` 指针在不同阶段指向不同内存区域，实现模块间解耦。

### 6.3 嵌入式性能优化策略汇总

| 策略 | 应用场景 | 效果 |
|------|---------|------|
| 查找表（LUT） | YUV→RGB 系数预计算 | 用 10KB 内存换数十倍速度 |
| 定点数运算 | 系数×1000 转整数 | 消除浮点运算 |
| 位移代替除法 | `>>8` 代替 `÷256` | 指令级加速 |
| 指针类型匹配数据宽度 | `unsigned short *` 读16位像素 | 一次读写完整像素 |
| FourCC 整型传参 | 传递像素格式标识符 | 减少栈开销 |
| CLIP 限幅宏 | 像素值安全保护 | 防止溢出导致颜色错误 |

---

## 七、数据流全景图

```
摄像头(V4L2)
    │
    ├── RGB565 ──→ rgb2rgb ──→ RGB565/RGB32
    │
    ├── YUV422 ──→ yuv2rgb ──→ RGB565/RGB32
    │                (查找表加速)
    │
    └── MJPEG ──→ mjpeg2rgb ──→ RGB565/RGB32
                     (libjpeg解码)
                          │
                          ▼
                   显示(LCD framebuffer)
```
