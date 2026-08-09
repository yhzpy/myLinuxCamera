---
kind: dependency_management
name: 基于Makefile的嵌入式C项目依赖管理（系统库与源码内嵌）
category: dependency_management
scope:
    - '**'
source_files:
    - video2lcd/Makefile
    - video2lcd/Makefile.build
    - video2lcd/video/Makefile
    - video2lcd/display/Makefile
    - video2lcd/convert/Makefile
    - video2lcd/render/Makefile
    - video2lcd/convert/jpeglib.h
    - video2lcd/convert/jdatasrc-tj.c
    - video2lcd/video/v4l2.c
---

## 1. 使用的系统与工具

本项目是纯 C 语言嵌入式 Linux 应用，**没有使用任何高级包管理器**（无 `go.mod`、`package.json`、`Cargo.toml`、`requirements.txt`、`pom.xml` 等）。依赖管理完全通过 **GNU Make + 交叉编译工具链** 实现：

- 顶层 `video2lcd/Makefile` 定义交叉编译器前缀 `arm-linux-`（`CC=arm-linux-gcc`、`LD=arm-linux-ld`、`AR=arm-linux-ar` 等），并导出到子目录构建。
- 顶层 `LDFLAGS := -lm -ljpeg` 声明外部动态库依赖：数学库 `libm` 和 JPEG 解码库 `libjpeg`。
- `video2lcd/Makefile.build` 实现递归子目录构建：解析各子模块 `Makefile` 中的 `obj-y += xxx.o`，将目标文件链接为 `built-in.o`，最终由顶层 `$(CC) -o $(TARGET) built-in.o $(LDFLAGS)` 生成可执行文件。
- 依赖头文件通过 `-I $(shell pwd)/include` 统一引入 `video2lcd/include/` 下的公共头文件（`config.h`、`convert_manager.h`、`disp_manager.h`、`pic_operation.h`、`render.h`、`video_manager.h`）。

## 2. 关键文件与第三方依赖

| 依赖类型 | 位置/声明方式 | 说明 |
|---|---|---|
| 系统库 | `video2lcd/Makefile` 中 `LDFLAGS := -lm -ljpeg` | 运行时链接 `libm`、`libjpeg`，需目标板已安装对应 `.so` |
| V4L2 / Framebuffer / mmap | `#include <sys/types.h>`、`<fcntl.h>`、`<sys/mman.h>`、`<poll.h>` 等 | 直接调用 Linux 内核接口，无需额外库 |
| JPEG 解码器源码 | `video2lcd/convert/jdatasrc-tj.c`、`jerror.h`、`jinclude.h`、`jpeglib.h` | 将 libjpeg 的部分源码以源码形式内嵌进项目，随 `convert/Makefile` 一起编译 |
| 内部模块间依赖 | `obj-y += convert/ display/ render/ video/` | 通过 `Makefile.build` 递归收集各子模块的 `*.o` 链接 |

## 3. 架构与约定

- **分层模块化**：每个功能域（`video`、`display`、`convert`、`render`、`render/operation`）各自拥有独立 `Makefile`，仅用 `obj-y += *.o` 声明参与构建的目标，由顶层 `Makefile.build` 统一聚合。这种设计使新增模块只需在顶层 `obj-y` 加一行即可被纳入构建。
- **头文件隔离**：所有对外暴露的接口集中在 `video2lcd/include/`，模块之间通过 `<xxx.h>` 引用，不直接包含系统头文件以外的路径。
- **源码内嵌策略**：JPEG 解码相关源文件（`jpeglib.h`、`jerror.h`、`jinclude.h`、`jdatasrc-tj.c`）直接放在 `convert/` 目录下，作为本地源码参与编译，而不是通过系统包管理器获取；其余部分仍依赖系统 `libjpeg`（由 `-ljpeg` 链接）。
- **交叉编译优先**：所有构建变量通过 `export` 传递给子 make，确保子模块也使用同一套 `arm-linux-*` 工具链。

## 4. 约定与约束

- **外部库版本控制**：未使用锁文件或版本锁定机制；`libm`、`libjpeg` 的版本由目标嵌入式系统的运行时环境决定，构建期只声明符号名（`-lm -ljpeg`），不固定具体版本。
- **私有仓库/镜像**：未发现任何私有源、GOPRIVATE、npm registry 或 pip index 配置；项目不依赖网络下载依赖。
- **依赖更新方式**：升级 `libjpeg` 需要手动替换 `convert/` 下的内嵌源码文件，并在目标板上安装对应版本的 `.so`；V4L2/Framebuffer 等内核接口则跟随目标板内核版本。
- **构建产物**：增量编译通过 `-Wp,-MD,$(dep_file)` 生成 `.d` 依赖文件（见 `Makefile.build` 第 43-44 行），用于跟踪头文件变更。
- **清理规则**：`clean` 删除所有 `*.o`，`distclean` 额外删除 `*.d` 和最终二进制，保持构建目录干净。

总结：该项目采用传统的嵌入式 C 项目依赖管理模式——**Makefile + 交叉编译工具链 + 源码内嵌 + 系统库动态链接**，没有引入现代包管理器，依赖版本由目标运行环境而非构建脚本锁定。