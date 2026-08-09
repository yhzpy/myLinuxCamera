---
kind: logging_system
name: 基于 DBG_PRINTF 宏的极简调试输出
category: logging_system
scope:
    - '**'
source_files:
    - video2lcd/include/config.h
---

## 1. 使用的系统/方法
本项目没有引入任何第三方日志框架（如 log4c、syslog、spdlog 等），也没有独立的 `log/` 或 `logging/` 目录。整个项目的“日志”仅依赖标准 C 库的 `printf`，并通过一个全局宏 `DBG_PRINTF` 统一封装。

## 2. 关键文件
- `video2lcd/include/config.h`：唯一与日志相关的定义处，提供 `DBG_PRINTF` 宏以及关闭调试输出的开关方式。

## 3. 架构与约定
- **单点封装**：所有模块通过包含 `include/config.h` 使用 `DBG_PRINTF(...)` 进行输出，而不是直接调用 `printf`。当前实现为 `#define DBG_PRINTF printf`，即原样转发到标准输出。
- **可关闭调试**：注释掉第 15 行 `#define DBG_PRINTF printf` 并保留空宏 `//#define DBG_PRINTF(...)` 即可在编译期消除所有 `DBG_PRINTF` 调用，达到关闭调试输出的目的。这是一种常见的嵌入式“零开销”开关策略。
- **无日志级别**：代码中未区分 ERROR/WARN/INFO/DEBUG 等层级，所有调试信息一律通过同一宏输出，无法按级别过滤。
- **无结构化字段**：输出均为裸字符串拼接，没有统一的键值对、时间戳、线程/进程 ID、模块名等结构化字段。
- **无独立 sink**：日志直接写入标准输出（stdout），没有重定向到文件、串口、syslog 或其他后端。

## 4. 实际使用情况
在项目中仅有少量位置使用了该宏或直接 `printf`：
- `convert/convert_manager.c`：打印支持的格式名称列表。
- `display/disp_manager.c`：打印显示设备名称列表。
- `video/video_manager.c`：打印视频操作接口名称。
- `main.c`：打印用法提示。
这些输出都是面向开发者的调试/诊断信息，而非生产环境日志。

## 5. 约束与规则
- **规则（由实现强制）**：若需关闭调试输出，应通过注释掉 `config.h` 中的 `#define DBG_PRINTF printf` 这一行来实现；这是当前仓库中唯一的“日志开关”。
- **约束（由现有代码体现）**：项目未定义任何日志级别枚举、日志上下文结构体或日志初始化函数，因此新增日志时只能沿用现有的 `DBG_PRINTF` + `printf` 模式。
- **限制**：由于没有日志级别和结构化字段，无法按严重性过滤或做集中式日志聚合；所有调试输出均进入 stdout，在生产部署时需要依赖外部机制（如 shell 重定向）来隔离日志流。

总结：这是一个极简的嵌入式调试输出方案——以 `DBG_PRINTF` 宏作为唯一入口，通过编译期宏开关控制是否输出，适合资源受限的嵌入式 Linux 相机应用，但不具备现代日志系统的分级、结构化与多后端能力。