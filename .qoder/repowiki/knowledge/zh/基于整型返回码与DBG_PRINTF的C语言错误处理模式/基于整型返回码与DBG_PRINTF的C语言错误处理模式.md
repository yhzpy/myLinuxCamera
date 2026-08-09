---
kind: error_handling
name: 基于整型返回码与DBG_PRINTF的C语言错误处理模式
category: error_handling
scope:
    - '**'
source_files:
    - video2lcd/include/config.h
    - video2lcd/video/v4l2.c
    - video2lcd/display/disp_manager.c
    - video2lcd/convert/convert_manager.c
    - video2lcd/convert/mjpeg2rgb.c
    - video2lcd/convert/yuv2rgb.c
    - video2lcd/convert/rgb2rgb.c
    - video2lcd/main.c
    - video2lcd/convert/jerror.h
---

## 1. 系统/方法概述

本项目是一个纯 C 语言的嵌入式 Linux 应用，没有使用任何高级错误处理框架（如异常、panic/recover、结构化错误类型）。整体采用**传统 C 风格**的错误处理：
- 函数通过返回 `int` 表示成功（`0`）或失败（负数，通常为 `-1`）。
- 错误信息通过 `DBG_PRINTF` 宏输出到标准输出（默认映射到 `printf`），用于调试和定位问题。
- 资源释放统一通过 `goto err_exit` 标签集中清理。
- 无全局 errno 语义传递，调用方需检查返回值并自行决定退出或继续。

该模式贯穿视频采集（V4L2）、显示（Framebuffer）、格式转换（YUV/MJPEG→RGB）、渲染（缩放/合并）等所有模块。

## 2. 关键文件与位置

- **配置层**：`video2lcd/include/config.h` — 定义 `DBG_PRINTF` 宏（默认展开为 `printf`），可通过注释关闭调试输出；定义设备路径常量（如 `FB_DEVICE_NAME`）。
- **视频采集**：`video2lcd/video/v4l2.c` — V4L2 设备初始化、流式/读写模式取帧、缓冲区 mmap、ioctl 调用，全部以 `iError` 变量 + `goto err_exit` 管理错误路径。
- **显示管理**：`video2lcd/display/disp_manager.c` — 显示设备注册链表、VideoMem 分配/获取/释放，内存分配失败返回 `-1`。
- **格式转换**：`video2lcd/convert/convert_manager.c` + `mjpeg2rgb.c` / `yuv2rgb.c` / `rgb2rgb.c` — 转换器注册、查找、执行，各转换函数返回 `0` 成功 / `-1` 失败。
- **主流程**：`video2lcd/main.c` — 编排 DisplayInit → VideoInit → VideoDeviceInit → StartDevice → GetFrame → Convert → Zoom → Merge → Flush → PutFrame 完整流水线，每步均检查返回值，失败则打印错误并 `return -1` 或 `continue`。
- **第三方库错误码**：`video2lcd/convert/jerror.h` — JPEG 库自带的错误消息宏（`JMESSAGE`），仅被 JPEG 解码子模块使用，不属于项目自定义错误体系。

## 3. 架构与约定

### 3.1 统一的整型返回约定
所有对外暴露的 API 遵循同一约定：
- 成功：返回 `0`
- 失败：返回 `< 0` 的整数（常见为 `-1`）
- 特殊查询：如 `GetDispResolution` 在未选择显示模块时返回 `-1`，`GetVideoConvertForFormats` 找不到转换器返回 `NULL`（指针而非整型）

### 3.2 错误日志输出
- 所有错误路径统一通过 `DBG_PRINTF("...\n")` 输出带上下文的描述性信息（如 `"Unable to set format"`、`"poll error!"`、`"No suitable video convert for formats: %d -> %d"`）。
- `DBG_PRINTF` 在 `config.h` 中定义为 `printf`，可通过取消注释 `#define DBG_PRINTF(...)` 来全局关闭调试输出，适合生产环境裁剪。
- 没有日志级别（INFO/WARN/ERROR）区分，也没有结构化日志。

### 3.3 资源清理模式
- 在 `v4l2.c` 的 `V4l2InitDevice` 中，所有失败分支统一跳转到 `err_exit:` 标签，先 `close(iFd)` 再 `return -1`，确保文件描述符不会泄漏。
- 其他模块（如 `disp_manager.c` 的 `AllocVideoMem`）在 `malloc` 失败时直接 `return -1`，由上层负责后续清理。

### 3.4 错误传播策略
- **致命错误**（初始化阶段）：DisplayInit、VideoDeviceInit、StartDevice 等失败后 `main` 直接 `return -1` 终止程序。
- **可恢复错误**（采集循环内）：`GetFrame` 失败时 `continue` 跳过当前帧，尝试下一帧，避免单帧错误导致整个进程崩溃。
- **不可恢复错误**（如格式不支持）：找不到匹配的转换器时打印错误并 `return -1`。

### 3.5 模块接口抽象中的错误处理
- `VideoOpr` / `DispOpr` / `VideoConvert` 三个结构体均以函数指针封装具体实现，错误由底层实现返回，上层统一按 `0`/非 `0` 判断。
- `Register*` 系列函数目前不检查参数合法性，总是返回 `0`，属于“宽松注册”设计。

## 4. 约定与约束

| 规则 | 说明 | 依据 |
|------|------|------|
| 函数必须返回 `int` 表示成功/失败 | 所有模块 API 统一用 `0` 表示成功，负数表示失败 | 全代码库一致实践 |
| 错误信息必须通过 `DBG_PRINTF` 输出 | 禁止裸 `printf` 或 `fprintf(stderr, ...)` | 便于全局开关调试输出 |
| 资源申请失败返回 `-1` | `malloc`、`open`、`mmap` 等失败统一返回 `-1` | `disp_manager.c`、`v4l2.c` 多处实现 |
| ioctl/mmap/poll 等系统调用失败必须检查返回值 | 每个系统调用后都有 `if (iError) { DBG_PRINTF; goto err_exit; }` | `v4l2.c` 每一处 ioctl 调用 |
| 采集循环中 I/O 错误应 `continue` 而非退出 | `GetFrame` 失败时 `continue` 等待下一帧 | `main.c` 第 120-126 行 |
| 初始化阶段的错误应直接退出 | 初始化失败后 `main` 立即 `return -1` | `main.c` 初始化序列 |
| 未找到匹配转换器返回 `NULL` | 指针类型的“空结果”作为错误信号 | `convert_manager.c` 的 `GetVideoConvertForFormats` |

## 5. 缺失与改进空间

- 没有统一的错误码枚举（如 `enum ErrorCode`），错误值散落在各处，难以区分不同类别的错误。
- 没有错误上下文链（如保存 errno 或上游错误原因），错误信息仅靠字符串描述。
- 没有 `setjmp/longjmp` 或 `signal` 机制处理硬件异常（如段错误），依赖内核默认行为。
- `jerror.h` 是 libjpeg 的头文件，包含 JPEG 库自身的错误消息宏，与项目自定义错误体系无关。

总体而言，该项目采用最朴素但有效的 C 语言错误处理模式：**整型返回码 + 描述性日志 + goto 统一清理**，适合嵌入式实时视频流的简单可靠场景。