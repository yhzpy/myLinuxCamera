---
kind: build_system
name: 基于Kbuild风格的递归Makefile构建系统
category: build_system
scope:
    - '**'
source_files:
    - video2lcd/Makefile
    - video2lcd/Makefile.build
    - video2lcd/video/Makefile
    - video2lcd/display/Makefile
    - video2lcd/render/Makefile
    - video2lcd/convert/Makefile
---

## 构建系统概述

该项目采用**类 Linux Kbuild 的递归 Makefile 构建系统**，用于在交叉编译环境下将嵌入式 Linux 相机采集与 LCD 显示应用（video2lcd）从多个子模块编译并链接为单一可执行文件。

## 核心架构

### 顶层入口：`video2lcd/Makefile`
- 定义交叉编译工具链前缀 `CROSS_COMPILE = arm-linux-`，导出 `CC/AS/LD/AR/NM/STRIP/OBJCOPY/OBJDUMP` 等工具变量供子模块复用。
- 设置全局编译选项：`CFLAGS := -Wall -O2 -g`，并通过 `-I $(shell pwd)/include` 暴露公共头文件目录。
- 链接依赖：`LDFLAGS := -lm -ljpeg`，表明应用使用数学库和 JPEG 解码库。
- 通过 `obj-y += main.o convert/ display/ render/ video/` 声明参与构建的目标对象与子目录。
- 最终链接命令：`$(CC) -o $(TARGET) built-in.o $(LDFLAGS)`，将所有目标打包成 `built-in.o` 后再链接。

### 递归引擎：`video2lcd/Makefile.build`
- 实现 Kbuild 风格的核心逻辑：解析顶层 `Makefile` 中的 `obj-y`，分离出普通 `.o` 文件和子目录。
- 对每个子目录递归调用 `make -C $@ -f $(TOPDIR)/Makefile.build`，形成统一的递归构建流程。
- 生成依赖文件：通过 `-Wp,-MD,$(dep_file)` 自动生成 `.d` 依赖追踪文件，支持增量编译。
- 汇总所有子目标：`built-in.o : $(cur_objs) $(subdir_objs)`，使用 `$(LD) -r` 将多个 `.o` 重定位合并为一个中间目标。

### 子模块 Makefile 约定
每个子目录仅声明本模块贡献的对象文件，遵循极简约定：
- `video/Makefile`：`obj-y += video_manager.o v4l2.o`
- `display/Makefile`：`obj-y += disp_manager.o fb.o`
- `convert/Makefile`：`obj-y += convert_manager.o yuv2rgb.o color.o mjpeg2rgb.o jdatasrc-tj.o rgb2rgb.o`
- `render/Makefile`：`obj-y += operation/`（进一步嵌套到 `operation/` 子目录）

## 构建流程

1. 执行 `make all` → 触发 `start_recursive_build` → 调用 `make -C ./ -f Makefile.build`
2. `Makefile.build` 解析 `obj-y`，递归进入各子目录执行相同构建脚本
3. 每个子目录生成自己的 `built-in.o`（或 `.o` 文件）
4. 顶层将全部目标用 `ld -r` 合并为 `built-in.o`
5. 最后用 `gcc` 链接 `built-in.o` + `LDFLAGS` 生成 `video2lcd` 可执行文件

## 清理规则
- `clean`：删除所有 `.o` 文件和最终目标
- `distclean`：额外删除 `.d` 依赖文件

## 关键约束与约定
- 所有子模块必须使用 `obj-y += xxx.o` 形式声明参与构建的对象，这是整个递归构建系统的契约。
- 公共头文件统一放在 `include/` 目录，通过顶层 `CFLAGS` 的 `-I` 路径暴露。
- 交叉编译环境由 `CROSS_COMPILE` 变量集中控制，便于切换目标平台工具链。
- 依赖自动追踪通过 GCC 的 `-Wp,-MD` 选项实现，无需手动维护依赖关系。
- 项目未包含 Docker、CI 流水线或发布脚本，构建完全依赖本地 Makefile。