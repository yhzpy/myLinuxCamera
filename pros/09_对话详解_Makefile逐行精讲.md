# 09 · 对话详解 · Makefile 逐行逐变量精讲

> 本篇整理自一次针对 **构建系统（`Makefile` + `Makefile.build`）** 的深入答疑对话。项目采用 **仿 Linux 内核 Kbuild 风格的递归 Makefile**，对新手极不友好——满屏的 `$(...)`、`%`、`$@`、`$^` 看着像天书。
>
> 本篇的目标：**先补 Makefile 语法基础，再逐行、逐个变量把两个文件讲透**。哪怕你完全没写过 Makefile，跟着看也能懂。
>
> 对应源码：`video2lcd/Makefile`、`video2lcd/Makefile.build`、各子目录 `Makefile`。

---

## 目录

- [第一部分：Makefile 语法速成（先学这个）](#第一部分makefile-语法速成)
- [第二部分：关于注释里的 a.o b.o c/ d/ 占位符](#第二部分关于注释里的-ao-bo-c-d-占位符)
- [第三部分：顶层 Makefile 逐行精讲](#第三部分顶层-makefile-逐行精讲)
- [第四部分：Makefile.build 前半段（1~28行）逐行逐变量](#第四部分makefilebuild-前半段128行逐行逐变量)
- [第五部分：Makefile.build 后半段（25~46行）逐行逐变量](#第五部分makefilebuild-后半段2546行逐行逐变量)
- [第六部分：自动变量 $@ $^ $< 速查](#第六部分自动变量---速查)
- [第七部分：整体构建流程图](#第七部分整体构建流程图)
- [第八部分：这套设计的好处](#第八部分这套设计的好处)

---

<a name="第一部分makefile-语法速成"></a>
## 第一部分：Makefile 语法速成

在解释具体代码前，必须先懂这 6 个基本语法，否则看什么都像天书。

### 语法 1：变量赋值 `:=`、追加 `+=`、延迟赋值 `=`

Makefile 里变量就是"给一串文字起个名字"。

```makefile
name := hello      # 定义变量 name，内容是 hello
```
- `name` 是变量名
- `:=` 读作"**赋值**"（把右边放进左边）
- **取用变量**：`$(变量名)`，如 `$(name)` 代表 `hello`

```makefile
name += world      # += 读作"追加"，在原内容后面加
```
执行后 `$(name)` = `hello world`。

| 符号 | 含义 | 展开时机 | 例子 |
|------|------|---------|------|
| `:=` | 赋值（覆盖） | **立即**展开右边 | `x := 1` → x 变成 1 |
| `+=` | 追加 | 跟随原变量 | `x += 2` → x 变成 "1 2" |
| `=` | 延迟赋值 | **用到时**才展开右边 | `x = $(y)`，x 的值取决于用它的时刻 y 是什么 |

> `=` 和 `:=` 的区别很关键：`:=` 在定义那一刻就把右边算死；`=` 会拖到真正使用变量时才计算右边。第 41 行 `dep_file = .$@.d` 用 `=` 就是这个原因（后面讲）。

### 语法 2：规则（目标 : 依赖 + 命令）

Makefile 的核心：

```makefile
目标 : 依赖
	命令
```

**生活例子——做三明治：**
```makefile
三明治 : 面包 鸡蛋
	把鸡蛋夹进面包
```
读法：我要做"三明治"（目标），需要"面包、鸡蛋"（依赖）先备好，备好后执行"把鸡蛋夹进面包"（命令）。

⚠️ **命令行前面必须是 Tab 键**（不是空格！），这是 Makefile 硬规定，用空格会报错。

### 语法 3：内置函数 `$(函数名 参数)`

先记住后面会反复用到的 5 个：

**① `$(filter 模式, 内容)` —— 筛选出匹配的**
```makefile
$(filter %/, a.o b.o c/ d/)   # → c/ d/
```
- `%/` 是模式，`%` 是通配符（代表任意字符），`%/` 意为"**以斜杠结尾的**"
- 从 `a.o b.o c/ d/` 筛出以 `/` 结尾的 → `c/ d/`

**② `$(filter-out 模式, 内容)` —— 剔除匹配的（相反）**
```makefile
$(filter-out %/, a.o b.o c/ d/)   # → a.o b.o
```
去掉以 `/` 结尾的 → `a.o b.o`。

**③ `$(patsubst 旧模式, 新模式, 内容)` —— 替换**
```makefile
$(patsubst %/, %, c/ d/)   # → c d
```
把 `c/ d/` 里符合 `%/`（带斜杠）的替换成 `%`（不带斜杠）→ `c d`。patsubst = pattern substitution。

**④ `$(foreach 变量, 列表, 表达式)` —— 循环**
```makefile
$(foreach f, c d, $(f)/built-in.o)   # → c/built-in.o d/built-in.o
```
把列表 `c d` 每个元素轮流赋给 `f`，展开表达式再拼起来。就像 for 循环："对每个 f，做这件事"。

**⑤ `$(wildcard 文件名)` —— 只返回真实存在的文件**
```makefile
$(wildcard .a.o.d .b.o.d)   # 只返回硬盘上存在的那些
```

### 语法 4：`include` —— 把另一个文件内容搬进来

```makefile
include Makefile
```
把当前目录下 `Makefile` 的全部内容，**原封不动"贴"到这一行来**。就像文档里写"此处插入另一文件全文"。

### 语法 5：条件判断 `ifneq ... endif`

```makefile
ifneq ($(dep_files),)
  include $(dep_files)
endif
```
- `ifneq (A, B)` = if not equal，"**如果 A 不等于 B**"
- `ifneq ($(dep_files),)` 右边是空的 → "**如果 dep_files 不等于空**"（即它有内容）
- `endif` 结束判断
- 大白话：**"如果 dep_files 里确实有东西，就把它包含进来"**

### 语法 6：伪目标 `.PHONY`

- 普通目标对应一个**真实文件**（如 `main.o` 真有这么个文件）
- **伪目标不对应真实文件**，只是个"动作名字"（如 `clean`、`convert`）
- `.PHONY : 目标列表` 告诉 make："**这些是动作，每次都执行，别去比对同名文件新旧**"
- 作用：防止目录里正好有同名文件时，make 误以为"文件已存在，不用做了"而跳过

---

<a name="第二部分关于注释里的-ao-bo-c-d-占位符"></a>
## 第二部分：关于注释里的 a.o b.o c/ d/ 占位符

`Makefile.build` 第 10~13 行有一段**原作者写的注释**（`#` 开头，不执行，只是说明）：

```makefile
# obj-y := a.o b.o c/ d/
# $(filter %/, $(obj-y))   : c/ d/
# __subdir-y  : c d
# subdir-y    : c d
```

**很多人卡在这里：a、b、c、d 到底是啥？**

答案：它们**不是项目里真实的文件，是原作者随手编的假名字**，用来演示函数怎么工作。好比数学里"设有 a、b 两个数"——a、b 不是具体数字，只是代号。

对应到本项目的真实名字：

| 注释里的假例子 | 项目里的真东西 | 是什么 |
|--------------|--------------|--------|
| `a.o` | `main.o` | 当前目录要编译的目标文件 |
| `b.o` | （顶层没有第二个，子目录里有 color.o 等） | 目标文件 |
| `c/` | `convert/` | 一个子目录 |
| `d/` | `display/` | 另一个子目录 |

所以注释里的 `obj-y := a.o b.o c/ d/`，在项目顶层**实际就是**（`Makefile` 第 30~34 行）：
```makefile
obj-y = main.o convert/ display/ render/ video/
```

**为什么例子要区分 a.o 和 c/？** 因为这段代码的核心任务就是**把清单分两类**：
```
obj-y = a.o  b.o  c/  d/
        └──┬──┘   └─┬─┘
      不带斜杠     带斜杠
      （.o文件）  （子目录）
```
- **带 `/`**（`c/`、`d/`）→ 子目录 → 要递归进去编译
- **不带 `/`**（`a.o`、`b.o`）→ 当前目录目标文件 → 直接编译

> 记住：**下文凡是出现 a.o b.o c/ d/ 的地方，都可以在脑子里替换成 main.o、convert/、display/ 这些真名字。**

---

<a name="第三部分顶层-makefile-逐行精讲"></a>
## 第三部分：顶层 Makefile 逐行精讲

先看 `video2lcd/Makefile`（54 行）的关键部分。

### 1. 交叉编译工具链定义（第 2~15 行）

```makefile
CROSS_COMPILE ?= arm-linux-gnueabihf-
AS  = $(CROSS_COMPILE)as
LD  = $(CROSS_COMPILE)ld
CC  = $(CROSS_COMPILE)gcc
...
export AS LD CC CPP AR NM
```

- `CROSS_COMPILE ?= arm-linux-gnueabihf-`：
  - `?=` 意为"**如果这个变量还没被定义，才赋这个值**"（可被外部环境变量覆盖）
  - `arm-linux-gnueabihf-` 是**交叉编译器前缀**。因为开发机是 x86（Windows/Linux PC），目标板是 ARM，不能直接用本机 gcc，要用能生成 ARM 代码的交叉编译器
- `CC = $(CROSS_COMPILE)gcc`：拼上前缀 → `CC = arm-linux-gnueabihf-gcc`（真正用来编译 .c 的编译器）
- `LD = $(CROSS_COMPILE)ld`：链接器
- `export ...`：**把这些变量导出给子 make**（因为后面要递归调用 make，子进程需要继承这些变量）

### 2. 编译选项（第 17~22 行）

```makefile
CFLAGS := -Wall -O2 -g
CFLAGS += -I $(shell pwd)/include
LDFLAGS := -lm -ljpeg
export CFLAGS LDFLAGS
```

- `CFLAGS`：编译选项
  - `-Wall`：开启所有常见警告
  - `-O2`：二级优化（提速）
  - `-g`：带调试信息（能用 gdb）
  - `-I $(shell pwd)/include`：头文件搜索路径。`$(shell pwd)` 是执行 shell 的 `pwd` 命令拿到当前绝对路径，拼上 `/include`
- `LDFLAGS`：链接选项
  - `-lm`：链接数学库（libm）
  - `-ljpeg`：链接 JPEG 库（libjpeg，MJPEG 解码要用）

### 3. 顶层目录与目标名（第 24~27 行）

```makefile
TOPDIR := $(shell pwd)
export TOPDIR
TARGET := video2lcd
```

- `TOPDIR`：项目根目录绝对路径，导出后子目录递归时靠它找到 `Makefile.build`
- `TARGET`：最终可执行文件名 `video2lcd`

### 4. obj-y 清单（第 30~34 行）——最关键

```makefile
obj-y += main.o
obj-y += convert/
obj-y += display/
obj-y += render/
obj-y += video/
```

- `obj-y` 是 Kbuild 的核心约定变量，意为"**本目录要构建的东西**"
- 用 `+=` 一句句追加，最终 `obj-y = main.o convert/ display/ render/ video/`
- **不带斜杠的 `main.o`** = 当前目录要编译的目标文件
- **带斜杠的 `convert/` 等** = 要递归进入的子目录

### 5. 三条规则（第 36~43 行）

```makefile
all : start_recursive_build $(TARGET)
	@echo $(TARGET) has been built!

start_recursive_build:
	make -C ./ -f $(TOPDIR)/Makefile.build

$(TARGET) : start_recursive_build
	$(CC) -o $(TARGET) built-in.o $(LDFLAGS)
```

**① `all` 目标（默认目标）**
- `all` 是第一条规则 = 默认目标，你直接敲 `make` 就是构建它
- 依赖两项，**从左到右顺序执行**：先 `start_recursive_build`（递归编译所有子目录），再 `$(TARGET)`（链接成最终程序）
- `@echo ...`：`@` 表示不回显命令本身，只打印"video2lcd has been built!"

> ⚠️ 注意：`pros/06_构建系统与C语言基础.md` 里曾把这条误写成 `ll :`，正确是 **`all`**。因为 Makefile 第一条规则才是默认目标，写成 `ll` 敲 `make` 就找不到标准入口了。

**② `start_recursive_build`（递归编译）**
```makefile
make -C ./ -f $(TOPDIR)/Makefile.build
```
- 伪目标，纯粹为执行命令
- `make`：再启动一个 make（**make 套 make = 递归**）
- `-C ./`：Change directory，切到当前目录再执行
- `-f $(TOPDIR)/Makefile.build`：**指定用 `Makefile.build` 当规则文件**，而非默认 Makefile
- 这一步是**真正编译所有 .c 文件**的地方

**③ `$(TARGET)`（最终链接）**
```makefile
$(CC) -o $(TARGET) built-in.o $(LDFLAGS)
```
代入变量展开：
```bash
arm-linux-gnueabihf-gcc -o video2lcd built-in.o -lm -ljpeg
```
- 依赖 `start_recursive_build` → 保证先把所有 .o 编译打包好，才链接
- `built-in.o`：所有源码打包成的那一个大 .o（递归编译的产物）

### 6. 清理规则（第 45~52 行）

```makefile
clean:
	rm -f $(shell find -name "*.o")
	rm -f $(TARGET)

distclean:
	rm -f $(shell find -name "*.o")
	rm -f $(shell find -name "*.d")
	rm -f $(TARGET)
```
- `clean`：删所有 .o 和可执行文件
- `distclean`：连 `.d` 依赖文件也删（更彻底）
- `$(shell find -name "*.o")`：执行 shell 的 find 命令，找出所有 .o 文件

---

<a name="第四部分makefilebuild-前半段128行逐行逐变量"></a>
## 第四部分：Makefile.build 前半段（1~28行）逐行逐变量

这是**通用构建引擎的"清单解析"阶段**——它不直接编译，而是先算出"要处理哪些东西"。

> 为了讲得具体，下文假设当前在**顶层目录**执行，此时 `obj-y = main.o convert/ display/ render/ video/`。

### 第 1~2 行：定义默认目标

```makefile
PHONY := __build
__build:
```
- 第1行：定义变量 `PHONY`，内容 `__build`（收集伪目标名，第46行会用）
- 第2行：定义目标 `__build`，是文件**第一条规则=默认目标**，用 `-f Makefile.build` 启动就从这开始。它的**依赖到第33行才补上**（Makefile 允许目标依赖分多次追加，这是 Kbuild 技巧）

### 第 5~6 行：清空变量

```makefile
obj-y :=
subdir-y :=
```
- 用 `:=` 把两个变量**清空**
- **为什么？** 因为下一行要 `include Makefile`，而每个目录的 Makefile 都用 `obj-y +=` 追加。先清空，保证拿到的是**当前目录自己的清单**，不受上层残留污染

### 第 8 行：读入当前目录的 Makefile（最关键）

```makefile
include Makefile
```
- 把**当前目录下的 `Makefile`** 全文贴进来
- 执行后 `obj-y` 被填上当前 Makefile 声明的内容（顶层就是 `main.o convert/ display/ render/ video/`）

> 巧妙处：**Makefile.build 是通用引擎，每个目录的 Makefile 只是喂给它的一份"清单（obj-y）"**。引擎不变，清单随目录不同而不同，从而实现递归构建。

### 第 14~15 行：从清单分离出"子目录"

```makefile
__subdir-y := $(patsubst %/,%,$(filter %/, $(obj-y)))
subdir-y   += $(__subdir-y)
```

**第14行从里往外算**：
1. 最里 `$(filter %/, $(obj-y))`：从 `main.o convert/ display/ render/ video/` 筛出带斜杠的 → `convert/ display/ render/ video/`
2. 外层 `$(patsubst %/,%, ...)`：去掉末尾斜杠 → `convert display render video`
3. `:=` 存进 `__subdir-y`

**第15行**：`+=` 把内容追加给 `subdir-y`，所以 `subdir-y = convert display render video`

> **意义**：把"子目录"单独挑出来。去斜杠是因为后面要 `make -C convert` 进目录，目录名不能带多余斜杠。

### 第 18 行：算出每个子目录的产物路径

```makefile
subdir_objs := $(foreach f,$(subdir-y),$(f)/built-in.o)
```
- `foreach` 遍历 `subdir-y`（4个元素），对每个 f 生成 `$(f)/built-in.o`：

| 轮次 | f | 展开结果 |
|-----|---|---------|
| 1 | convert | `convert/built-in.o` |
| 2 | display | `display/built-in.o` |
| 3 | render | `render/built-in.o` |
| 4 | video | `video/built-in.o` |

- 拼起来：`subdir_objs = convert/built-in.o display/built-in.o render/built-in.o video/built-in.o`

> **意义**：每个子目录编译完会产出一个打包文件 `built-in.o`。这里**提前算好路径**，第38行打包顶层 built-in.o 时拿它们当原料。

### 第 20~21 行：分离出"当前目录的 .o"

```makefile
# a.o b.o                              ← 注释（假例子），别管
cur_objs := $(filter-out %/, $(obj-y))
```
- `filter-out %/`：**剔除**带斜杠的（子目录）
- 从 `main.o convert/ display/ render/ video/` 剔掉所有带斜杠的 → `cur_objs = main.o`

对比记忆：
- 第14行 `filter %/` = **只保留**带斜杠的 → 子目录 `subdir-y`
- 第21行 `filter-out %/` = **剔除**带斜杠的 → .o 文件 `cur_objs`

清单被劈成两半：
```
obj-y = main.o  convert/  display/  render/  video/
          │        └──────────┴─────────┴────────┘
          │                    ↓ filter 挑带斜杠 + 去斜杠
          │        subdir-y = convert display render video
          ↓ filter-out 剔除带斜杠
     cur_objs = main.o
```

> 顶层 `cur_objs` 只有 `main.o`（顶层只有 main.c 一个源文件）。但进到 `convert/` 目录执行时，那边 Makefile 写的是 `obj-y += color.o yuv2rgb.o ...`，`cur_objs` 就会是一堆。

### 第 22~23 行：处理依赖文件 .d

```makefile
dep_files := $(foreach f,$(cur_objs),.$(f).d)
dep_files := $(wildcard $(dep_files))
```

**第22行**：`foreach` 遍历 `cur_objs`（顶层 `main.o`），对每个 f 生成 `.$(f).d`（**前面加点 `.`，后面加 `.d`**）：
- f=main.o → `.main.o.d`
- 结果 `dep_files = .main.o.d`

`.main.o.d` 是**依赖文件**（`.d`=dependency），编译时由编译器自动生成（靠第44行 `-Wp,-MD`），内容大概是：
```makefile
main.o : main.c include/config.h include/disp_manager.h ...
```
意思是"main.o 依赖 main.c 和这些头文件"。

**第23行**：`wildcard` **只保留硬盘上真实存在的**：
- 检查 `.main.o.d` 此刻存不存在：存在→保留；不存在→丢弃（`dep_files` 变空）
- **为什么非要这步？** 关键在"**第一次编译**"：

```
第一次 make（还没编译过）：
   .main.o.d 还没生成 → wildcard 发现不存在 → dep_files 变空
   → 第25行 ifneq 判断"是空的" → 跳过 include → 不报错 ✓

第二次 make（已编译过，.d 存在）：
   .main.o.d 存在 → wildcard 保留 → dep_files = .main.o.d
   → 第25行判断"非空" → include 进来 → 获得头文件依赖 ✓
```
没有第23行，第一次编译直接 `include .main.o.d` 会因文件不存在而**报错中断**。`wildcard` 就是兜这个底的。

### 第 25~27 行：有依赖文件才包含

```makefile
ifneq ($(dep_files),)
  include $(dep_files)
endif
```
- `ifneq ($(dep_files),)` = "如果 dep_files 不是空的"
- 成立才 `include $(dep_files)`，把 `.main.o.d` 里的依赖规则贴进来
- 贴进来后 make 就知道"main.o 依赖 config.h"之类信息——**你改了 config.h，main.c 会被自动重新编译**。这就是"自动依赖跟踪"，省得手动维护头文件关系

### 前半段小结

这 28 行**不编译任何代码，只是"整理清单、做准备"**。打个比方，装修房子开工前的材料清点：

1. **拿到采购清单**（include Makefile → obj-y）：混着"自己做的家具"和"外包工程"
2. **分两类**：外包工程（子目录）→ `subdir-y`；自己做的家具（当前.o）→ `cur_objs`
3. **记下外包交付物地址**（`convert/built-in.o` 等）→ `subdir_objs`
4. **翻出上次施工记录**（.d 依赖文件）→ include 进来

清点完，后半段才真正开工。

---

<a name="第五部分makefilebuild-后半段2546行逐行逐变量"></a>
## 第五部分：Makefile.build 后半段（25~46行）逐行逐变量

这是引擎**真正动手编译和打包**的地方。

### 第 30 行：把子目录登记为伪目标

```makefile
PHONY += $(subdir-y)
```
- `PHONY` 前面（第1行）已有 `__build`，这里 `+=` 追加 `subdir-y`
- 结果 `PHONY = __build convert display render video`
- 把子目录声明成伪目标，**防止 make 误判**：万一目录里有跟目标同名的文件，make 会以为"文件已存在不用做"而跳过。声明 PHONY 就是告诉 make"这些是动作，每次都执行"（第46行统一生效）

### 第 33 行：给默认目标补上依赖

```makefile
__build : $(subdir-y) built-in.o
```
- 还记得第2行的 `__build:` 吗？当时冒号后空的，这里**补上依赖**
- `__build` 依赖 `$(subdir-y)`（`convert display render video`）和 `built-in.o`
- 含义：**要完成 __build，得先处理好所有子目录，再生成 built-in.o**

### 第 35~36 行：递归进入子目录（引擎心脏）

```makefile
$(subdir-y):
	make -C $@ -f $(TOPDIR)/Makefile.build
```

**`$(subdir-y):` 其实一次性定义了 4 个目标**（共用下面的命令）：
```makefile
convert display render video:
	make -C $@ -f $(TOPDIR)/Makefile.build
```

**命令里的 `$@`**（自动变量）代表"**当前正在处理的那个目标的名字**"：
- 处理 convert 时 `$@`=convert → `make -C convert -f .../Makefile.build`
- 处理 display 时 `$@`=display → `make -C display -f .../Makefile.build`
- 以此类推

**`make -C convert -f $(TOPDIR)/Makefile.build` 拆解**：
- `make`：再开一个 make（**递归**）
- `-C convert`：切换到 `convert/` 目录里去
- `-f $(TOPDIR)/Makefile.build`：还用**同一个** Makefile.build 当规则（`$(TOPDIR)` 是顶层绝对路径，保证在子目录里也能找到它）

> **这一行就是"递归"的本质**：用同一个引擎钻进每个子目录再跑一遍。子目录里也有自己的 Makefile（声明自己的 obj-y），若里面还有子目录（如 `render/operation/`），会继续往下钻，直到最底层。

### 第 38~39 行：把当前目录所有 .o 打包成 built-in.o

```makefile
built-in.o : $(cur_objs) $(subdir_objs)
	$(LD) -r -o $@ $^
```

**依赖部分**：`built-in.o` 依赖两样（前面算好的变量）：
- `$(cur_objs)` = `main.o`（当前目录的 .o）
- `$(subdir_objs)` = `convert/built-in.o display/built-in.o render/built-in.o video/built-in.o`

**命令部分** `$(LD) -r -o $@ $^`，两个自动变量：
- `$@` = 目标名 = `built-in.o`
- `$^` = **所有依赖**（上面那 5 个 .o 全算上）

代入展开：
```bash
arm-linux-gnueabihf-ld -r -o built-in.o main.o convert/built-in.o display/built-in.o render/built-in.o video/built-in.o
```

**关键是 `-r` 参数**：
- `ld` 是链接器
- `-r` = **relocatable（可重定位）**，"把多个 .o 合并成一个 .o，但保留符号信息，以后还能继续链接"
- 普通 `ld` 直接生成可执行文件，而 `-r` 生成的"**还是一个 .o，只不过是合并版的大 .o**"

**层层汇聚效果**：
```
color.o yuv2rgb.o ...  → convert/built-in.o ┐
fb.o disp_manager.o    → display/built-in.o │
v4l2.o video_manager.o → video/built-in.o   ├→ 顶层 built-in.o → video2lcd
merge.o zoom.o ...     → render/built-in.o  │
main.o                                      ┘
```
每层都用 `-r` 把本层小 .o 合并，最后汇成顶层 `built-in.o`，交给主 Makefile 第43行链接成最终程序。

### 第 41 行：定义依赖文件名模板

```makefile
dep_file = .$@.d
```
- 用 `=`（**不是 `:=`**），是"**延迟赋值**"——右边 `$@` **不立刻展开**，等真正用到 `dep_file` 时才按当时的 `$@` 计算
- `$@` = 当前目标名。目标是 main.o 时，`dep_file` = `.main.o.d`
- **为什么用 `=` 不用 `:=`？** 因为 `$@` 在每个 .o 编译时都不一样。用 `:=` 会在定义时就把 `$@` 固定成空值；用 `=` 才能保证"每次编译不同文件时 `$@` 取到各自文件名"。这是配合下一行使用的

### 第 43~44 行：把 .c 编译成 .o（最底层干活规则）

```makefile
%.o : %.c
	$(CC) $(CFLAGS) -Wp,-MD,$(dep_file) -c -o $@ $<
```

**`%.o : %.c` 是"模式规则"**（`%` 是通配符）：
- 意思："**任何 .o 文件，都依赖同名的 .c 文件**"
- 要 `main.o` 就找 `main.c`；要 `color.o` 就找 `color.c`
- 一条规则搞定所有 .c→.o，不用一个个手写

**命令逐个拆**：
```bash
$(CC)               → arm-linux-gnueabihf-gcc      （交叉编译器）
$(CFLAGS)           → -Wall -O2 -g -I .../include  （编译选项）
-Wp,-MD,$(dep_file) → 生成依赖文件 .main.o.d
-c                  → 只编译不链接，产出 .o
-o $@               → 输出到目标名（$@ = main.o）
$<                  → 第一个依赖（$< = main.c）
```

两个自动变量：
- `$@` = 目标名 = `main.o`
- `$<` = **第一个依赖** = `main.c`（注意：`$<` 只取第一个，`$^` 取全部）

完整展开（编译 main.c 时）：
```bash
arm-linux-gnueabihf-gcc -Wall -O2 -g -I .../include -Wp,-MD,.main.o.d -c -o main.o main.c
```

**`-Wp,-MD,.main.o.d` 的作用**：让编译器在编译同时**自动生成 `.main.o.d` 依赖文件**，记下 main.o 依赖的所有头文件。这就跟第22、41行呼应上了——`.d` 文件在这里产生，下次编译时被 include 进来实现自动依赖跟踪。

### 第 46 行：正式声明所有伪目标

```makefile
.PHONY : $(PHONY)
```
- `.PHONY` 是 make 的**特殊内置目标**，声明"哪些是伪目标"
- `$(PHONY)` 前面（第1、30行）攒了 `__build convert display render video`
- 展开：`.PHONY : __build convert display render video`
- 作用：告诉 make 这几个都是"动作"，**每次都执行，别去比对同名文件**

---

<a name="第六部分自动变量---速查"></a>
## 第六部分：自动变量 $@ $^ $< 速查

这三个是 make 的"自动变量"，在规则里自动代表不同东西，**务必记住**：

| 变量 | 含义 | 在 `built-in.o : main.o convert/built-in.o` 里的值 |
|------|------|------|
| `$@` | **目标**名 | `built-in.o` |
| `$^` | **所有**依赖 | `main.o convert/built-in.o`（全部） |
| `$<` | **第一个**依赖 | `main.o`（只第一个） |

记忆口诀：
- **`$@`** 像"靶心" = 目标（@ 有"at 那个东西"的意味）
- **`$^`** 像"全部升起" = 所有依赖
- **`$<`** 像"箭头指第一个" = 首个依赖

在本项目里的实际用法：
- 第36行 `make -C $@ ...`：`$@` = 当前子目录名（convert/display/...）
- 第39行 `$(LD) -r -o $@ $^`：`$@` = built-in.o，`$^` = 所有要合并的 .o
- 第44行 `... -o $@ $<`：`$@` = 目标 .o，`$<` = 对应的源 .c

---

<a name="第七部分整体构建流程图"></a>
## 第七部分：整体构建流程图

从你敲下 `make` 到生成 `video2lcd` 的完整过程：

```
你敲 make
   ↓
all 目标（顶层 Makefile 第一条规则）
   ↓
① start_recursive_build
   ↓  make -C ./ -f Makefile.build
② Makefile.build 读 obj-y，分拣出子目录 convert/display/render/video
   ↓  对每个子目录 make -C 子目录 -f Makefile.build（第35-36行，递归）
③ 递归到最底层：%.o:%.c（第43-44行）把每个 .c 编译成 .o，顺便生成 .d 依赖文件
   ↓  $(LD) -r（第38-39行）逐层合并
④ 子目录.o → 子目录 built-in.o → ... → 顶层 built-in.o
   ↓
⑤ $(TARGET) 目标（顶层 Makefile 第42-43行）：
   gcc -o video2lcd built-in.o -lm -ljpeg（最终链接）
   ↓
⑥ 生成可执行文件 video2lcd，打印 "video2lcd has been built!"
```

递归的层级示意（以 render 目录为例，它下面还有 operation 子目录）：

```
顶层 Makefile.build
 ├── convert/  → color.o yuv2rgb.o mjpeg2rgb.o rgb2rgb.o convert_manager.o
 │                 └─(ld -r)→ convert/built-in.o
 ├── display/  → fb.o disp_manager.o
 │                 └─(ld -r)→ display/built-in.o
 ├── video/    → v4l2.o video_manager.o
 │                 └─(ld -r)→ video/built-in.o
 ├── render/   → render/operation/ → merge.o zoom.o
 │                 │        └─(ld -r)→ render/operation/built-in.o
 │                 └─(ld -r)→ render/built-in.o
 └── main.o
       ↓ 全部 (ld -r) 合并
   顶层 built-in.o
       ↓ gcc 链接
   video2lcd
```

---

<a name="第八部分这套设计的好处"></a>
## 第八部分：这套设计的好处（面试可讲）

1. **模仿 Linux 内核 Kbuild**：`obj-y`、`built-in.o`、`make -C ... -f Makefile.build` 全是内核构建系统的写法。内核几万个文件也能高效构建，就靠这套。能在面试里讲清 Kbuild 机制，是工程能力的加分项。

2. **模块化、解耦**：每个子目录只管自己的 `Makefile`（声明 `obj-y`），不用关心别人。加一个模块目录，只需在父级 `obj-y += 新目录/` 一行。

3. **增量编译 + 自动依赖**：靠 `.d` 文件，只重编改动过的 .c，且头文件变了也能感知，构建快且准。

4. **`-r` 分层打包**：把"很多小 .o"逐层合并成"一个 built-in.o"，最后只链接一次，链接命令简洁。

5. **引擎与清单分离**：`Makefile.build` 是**通用引擎**（所有目录共用），各目录 `Makefile` 只是**清单**（声明 obj-y）。改构建逻辑只改一处，改模块组成只改各自清单。这是"机制与策略分离"的思想。

---

## 附：一句话总览

> **顶层 Makefile** 定义工具链（交叉编译器）、编译选项、`obj-y` 清单和三条规则（all / start_recursive_build / $(TARGET)）；**Makefile.build** 是所有目录共用的通用引擎：前 28 行"解析清单"（include Makefile 拿 obj-y → filter/patsubst 分拣出子目录 `subdir-y` 和当前 .o `cur_objs` → foreach 算子目录产物 `subdir_objs` → wildcard+include 加载 .d 依赖）；后半段"真正干活"（第35-36行 `make -C $@` 递归进子目录，第43-44行 `%.o:%.c` 用 gcc 编译并生成 .d，第38-39行 `ld -r` 逐层合并成 built-in.o），全程靠 `$@`（目标）、`$^`（所有依赖）、`$<`（首个依赖）三个自动变量把规则写成通用模板。最终所有代码汇成一个顶层 `built-in.o`，链接成 `video2lcd`。

---

*本篇整理自构建系统答疑对话，配合 `06_构建系统与C语言基础.md` 一起复习效果更佳。*
