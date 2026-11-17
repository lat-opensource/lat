# LATU option 完整参考

本文记录 LATU 用户态翻译器的运行选项，保留代码中的 LATX/LAT 命名。
覆盖 `linux-user/main.c` 的全部命令行注册项，并补充 Quicktrans 路径选择和
没有公开设置入口的主要内部选项。Quicktrans 内部的临时诊断环境变量不在完整
注册表的范围内；它们不是稳定配置接口。

核对源码基线：`ef5d2bc6359ecbe2a663acfb8c0149b8f9c5fb53`。
默认值指未读取系统配置、用户配置、环境变量和命令行前的初始化值，
不是当前机器的有效配置，也不是性能或兼容性保证。

## 设置方式与限制

配置路径、按程序配置的方法以及优先级见 [LATX 配置参考](latx-environment.rst)。
同一个选项的覆盖顺序为：命令行 > 环境变量 > 用户配置 > 系统配置 > 内置默认。
不同选项之间还存在解析副作用，不能把这条优先级理解为依赖关系校验。
环境变量按注册表顺序解析，命令行按用户填写顺序解析。

```bash
# 一次运行：关闭指令组合优化
LATX_INSTPTN=0 latx-x86_64 /path/to/program
# 等价命令行写法；参数放在 guest 程序之前
latx-x86_64 -latx-instptn 0 /path/to/program
# 模式 2 的完整软浮点路径，不开启快速位
LATX_SOFTFPU=2 LATX_SOFTFPU_FAST=0 latx-x86_64 /path/to/program
```

多数整数用基数 0 解析，可使用十进制或带 0x 的十六进制；旧解析器未必拒绝
越界或无效字符。下表的 0/1 是开关用法，不表示每个处理函数都严格验证输入。
标为“无参数”的环境变量通常按“是否存在”启用，写成 `=0` 也可能启用；
要关闭应移除环境变量。

命令行与环境变量由 `arg_table` 注册，配置文件由头文件的 `ENVS` 另行注册。
表中“配置”表示源码有配置文件入口，仍受该功能的编译条件限制；“否”表示
不能把这个环境变量名直接写进配置文件。通用 LAT_* 配置入口多数仅调试构建提供，
但命令行/环境变量并不因此全部限定为调试构建。

以下历史入口在命令行注册为无参数，但处理函数实际需要字符串：
`-latx-optimize`、`-latx-unimp-dump`、`-latx-aot-file-size`、
`-latx-aot-left-file-size`、`-latx-runtime-trace-begin`、
`-latx-runtime-trace-end`。请使用对应环境变量或受支持的配置文件入口；
不要在这些命令后追加一个值。本次文档不修改解析代码。

## 翻译、浮点与库直通

| 环境变量 / 命令行 | 默认值；取值 | 配置 | 作用与限制 |
| --- | --- | --- | --- |
| `LATX_OPTIMIZE` / `-latx-optimize` | 未设置；tunnel-lib / vpaes | 有 | 启用库调用通道或向量 AES；只设置对应功能为真，不用于关闭。 |
| `LATX_VPAES` / `-latx-vpaes <值>` | 0；0/1 | 有 | 使用向量指令翻译 AES；需要相应向量支持，不代表所有 AVX AES 入口都走此路径。 |
| `LATX_SMC` / `-latx-smc <值>` | 0；0/1/2/6 | 有 | 自修改代码失效策略：0 按页，非零按翻译块；位 1 开共享内存监控，位 2 开 store helper。帮助文本中的 default 6 与初始化不符，以此处源码默认 0 为准。 |
| `LATX_CLOSE_PARALLEL` / `-latx-close-parallel <值>` | 0；0/1 | 有 | 非零关闭 LATX 并行相关处理；这是反向开关。 |
| `LATX_SOFTFPU` / `-latx-softfpu <值>` | 0；0/1/2 | 有 | 选择 x87 浮点路径：0 硬浮点，1/2 为两种软浮点实现。非零在解析时关闭 AOT 和 rounding 优化。 |
| `LATX_SOFTFPU_FAST` / `-latx-softfpu-fast <值>` | 0；位掩码 | 有 | 为软浮点模式 2 选择部分指令的快速路径，位定义见后文。并非完整软浮点语义的等价替换。 |
| `LATX_PRLIMIT` / `-latx-prlimit <值>` | 0；0/1 | 有 | 由 LATX 记录并检查 guest 的 RLIMIT_AS/RLIMIT_DATA，配合 mmap/brk 等操作处理内存限制。 |
| `LATX_ROUNDING_OPT` / `-latx-rounding <值>` | 1；0/1 | 有 | 减少浮点舍入模式的重复设置；软浮点选项会清零此值。 |
| `LATX_QUICKTRANS_RSP_MERGE` / `-latx-quicktrans-rsp-merge <值>` | 1；0/1 | 有 | 合并连续的栈指针 RSP 调整。仅 Quicktrans x86_64 构建提供；仍需满足块内分析条件。 |
| `LATX_INSTPTN` / `-latx-instptn <值>` | 0x3ffffff；0..0x3ffffff | 有 | 选择指令组合优化，0 全关。位与模式编号不是同一数值，见后文。 |
| `LATX_CVT_OPT` / `-latx-cvt-opt <值>` | 1；0/1 | 有 | 控制浮点转换指令优化；是否使用取决于具体指令路径。 |
| `LATX_AVX_CPUID` / `-latx-avx-cpuid <值>` | 1；0/1 | 有 | 控制 AVX 相关 CPUID 上报；仅 CONFIG_LATX_AVX_OPT 构建提供，不会删除已编译的 AVX 翻译代码。 |
| `LATX_KZT` / `-latx-kzt <值>` | 0；0/1/2 | 有 | 库直通总开关：0 关闭、1 按兼容性检查启用、2 强制测试模式；仅 KZT 构建提供。 |
| `LATX_KZT_GUEST_TLS` / `-latx-kzt-guest-tls <值>` | 0；0/1 | 有 | 为原生线程回调建立 guest 线程局部存储；仅 KZT 构建提供，开启要求 x86_64 guest。详见 docs/devel/kzt-guest-tls.md。 |
| `LATX_KZT_LIBS` / `-latx-kzt-libs <值>` | stable；库族列表 | 有 | 选择直通库族，支持 name 列表或 +name/-name 增减，不能混用。见配置参考的 KZT 库族控制。 |
| `LATX_KZT_LOG` / `-latx-kzt-log <值>` | 0；0/1 | 有 | 输出库直通绑定、依赖选择与降级原因；仅 KZT 构建提供。 |
| `LATX_FPUTAG` / `-latx-fputag <值>` | 0；0/1 | 有 | 控制硬浮点路径中的 x87 栈槽标签维护与检查。标签用于记录浮点栈槽是否为空；并非所有软浮点路径都受此开关控制。 |
| `SAVE_XMM` / `-save-xmm <值>` | 内部掩码 0xff；0/非零 | 有 | 非零把 option_save_xmm 设为 0xff；传 0 不清零，不能据此关闭保存。 |
| `LATX_JRRA` / `-latx-jrra <值>` | 0；0/1 | 有 | 控制返回地址相关优化；非零在解析时关闭 AOT，实际路径还取决于 JRRA/LSFPU 编译支持。 |
| `LATX_IMM_REG` / `-latx-imm-reg <值>` | 0000；四位字符串 | 有 | 依次控制普通立即数、RIP 相对地址、复杂寻址、预缓存；必须写满四位 0/1，例如 1000。需要 CONFIG_LATX_IMM_REG 对应实现。 |

## 内存、Wine 与进程

| 环境变量 / 命令行 | 默认值；取值 | 配置 | 作用与限制 |
| --- | --- | --- | --- |
| `LATX_MT` / `-latx-mem-test <值>` | 0；0/1/2 | 有 | 16 KiB 主机页上的访存检查：1 稀疏快速检查、2 严格快速检查。非 16 KiB 主机自动清零；有效开启会关闭 AOT。 |
| `LATX_MINKE_16K_PAGE_CHECK` / `-latx-minke-16k-page-check <值>` | 0；0/1 | 有 | 现有 Minke 专用混合页写检查，只在 32 位 guest、16 KiB 主机页下保留；还受程序匹配限制，不是通用内存检查开关；匹配生效后关闭 AOT。 |
| `LATX_REAL_MAPS` / `-latx-real-maps <值>` | 0；0/1 | 有 | 读取真实 self maps；非零在解析时关闭 AOT。 |
| `LATX_MONITOR_SHARED_MEM` / `-latx-monitor-shared-mem <值>` | 0；0/1 | 有 | 监控共享内存中代码的变化，触发重新翻译。 |
| `LATX_PRIVATE_MMAP_SHADOW` / `-latx-private-mmap-shadow <值>` | 0；0/1 | 有 | 为可写私有 SQLite 文件映射片段使用影子映射处理。 |
| `LATX_SMC_RELOAD` / `-smc_reload <值>` | 0；整数 | 否 | 控制 SMC 翻译块重装；非零在解析时关闭 option_jr_ra。命令名是 smc_reload，含下划线。 |
| `LATX_WINE_PE_FIXED_BASE` / `-latx-wine-pe-fixed-base <值>` | 未设置；basename | 否 | 为一个可重定位 Wine PE 映像优先使用它声明的基址。 |
| `LATX_WINE_PE_FIXED_ADDRESS` / `-latx-wine-pe-fixed-address <值>` | 未设置；basename@address | 否 | 为一个可重定位 Wine PE 映像指定装载地址；属于针对映射问题的诊断控制。 |
| `LATX_FORK_UNLINK` / `-latx-fork-unlink <值>` | 0；0/1 | 有 | 在 fork 前通过内部信号断开正在执行的翻译块连接，让线程退出直接跳转链。 |
| `LATX_ANONYM` / `-latx-anonym <值>` | 0；0/1 | 有 | 隐藏部分翻译环境特征；调用该解析器就清零 AOT，包括显式设置为 0，后续参数仍可能改变 AOT。 |
| `LATX_MMAP_START` / `-latx-mmap_start <值>` | TASK_UNMAPPED_BASE；地址 | 有 | 改变自动 mmap 的初始搜索地址；默认取决于目标 ABI。 |
| `LATX_MMAP_FIXED` / `-latx-mmap_fixed <值>` | 0；0/1 | 有 | 非零时，对带非零地址的 mmap 强制加入 MAP_FIXED，会改变原映射的替换行为。 |
| `LATX_UNIMP_DUMP` / `-latx-unimp-dump` | 未设置；日志路径 | 有 | 设置未实现操作的日志类别和输出路径；使用环境变量或配置文件传路径。 |

## AOT 缓存

本节全部要求 `CONFIG_LATX_AOT`。Quicktrans 构建的 AOT 默认值在编译时决定，
运行时设置 `LATX_FAST_TRANSLATOR=0` 不会把默认值恢复为 1。

| 环境变量 / 命令行 | 默认值；取值 | 配置 | 作用与限制 |
| --- | --- | --- | --- |
| `LATX_AOT` / `-latx-aot <值>` | 传统构建 1；Quicktrans 构建 0；0/1 | 有 | AOT 总开关。AOT 是把翻译结果保存到磁盘供后续运行使用；软浮点或有效 LATX_MT 会阻止开启。 |
| `LATX_LOAD_AOT` / `-latx-aot-load <值>` | 传统构建 1；Quicktrans 构建 0；0/1 | 否 | 在 AOT 开启时控制读取已有缓存；单独设 1 不会打开 AOT 总开关。 |
| `LATX_QUICKTRANS_AOT_COLLECT` / `-latx-quicktrans-aot-collect <值>` | 0；0/1 | 否 | 实验性：收集 Quicktrans 可执行文件映射，通过旧路径的退出处理保存快照并生成传统 AOT；适用退出方式与限制见后文。 |
| `LATX_AOT_GENERATE` / `-latx-aot-generate <值>` | -1；-1/0/1 | 否 | 普通 AOT 生成策略：自动、禁止、强制请求。独立的 Quicktrans 收集开关不受该值关闭影响。 |
| `LATX_AOT_PE_PROFILE` / `-latx-aot-pe-profile <值>` | 0；0/1 | 否 | 按进程角色拆分 libcef PE 的 AOT 配置。 |
| `LAT_AOT_FILE_SIZE` / `-latx-aot-file-size` | 未设置；MB 字符串 | 有 | 历史缓存大小参数；当前仓库仅保存参数，未发现消费该值的实现，不应据此认为缓存大小受限。 |
| `LAT_AOT_LEFT_FILE_SIZE` / `-latx-aot-left-file-size` | 未设置；MB 字符串 | 有 | 历史剩余空间参数；当前仓库仅保存参数，未发现消费该值的实现。 |
| `LATX_AOT_WINE_PEFILES_CACHE` / `-latx-aot-wine-pefiles-cache <值>` | 未设置；逗号分隔列表 | 有 | 选择 Wine PE 缓存对象，帮助示例为 .dll、.exe、.sys、.drv；需要 AOT 构建。 |

## LATX 调试选项

本节要求 `CONFIG_LATX_DEBUG` 或 `CONFIG_DEBUG_TCG`，并启用 LATX。

| 环境变量 / 命令行 | 默认值；取值 | 配置 | 作用与限制 |
| --- | --- | --- | --- |
| `LATX_BEGIN_TRACE` / `-latx-runtime-trace-begin` | 0；十六进制地址 | 有 | 逐指令运行跟踪的起始地址。 |
| `LATX_END_TRACE` / `-latx-runtime-trace-end` | 0；十六进制地址 | 有 | 逐指令运行跟踪的结束地址。 |
| `LATX_DUMP` / `-latx-dump <值>` | 00000；五位字符串 | 有 | 依次输出函数、IR1、IR2、主机指令、profile 信息；IR1/IR2 是翻译器中间表示。必须写满五位。 |
| `LATX_SHOW_TB` / `-latx-show-tb <值>` | 0；地址 | 有 | 按 guest PC 指定要显示的翻译块（TB，即一段一起翻译的指令）。 |
| `LATX_TRACE_MEM` / `-latx-trace-mem <值>` | 0；地址 | 有 | 记录最后写入指定地址的 guest 指令地址。 |
| `LATX_BREAK_INSN` / `-latx-break-insn <值>` | 0；地址 | 有 | 在指定 guest 指令处主动触发 SIGSEGV，供调试器截获。 |
| `LATX_DEBUG_LATIVE` / `-latx-debug-lative` | 关闭；无参数 | 有 | Lative 调试预设：设置显示和断点地址 0x30000，并启用调试标记。 |
| `LATX_ENABLE_FCSR_EXC` / `-latx-enable_fcsr_exc` | 关闭；无参数 | 有 | 启用主机浮点控制寄存器 FCSR 的异常位；调试用途。 |
| `LATX_UNLINK` / `-latx-unlink <值>` | 0；次数[,CPU编号] | 有 | 指定断开翻译块连接的计数和可选 CPU 编号。 |
| `LATX_TRACE` / `-latx-trace <值>` | 00；两位字符串 | 否 | 依次跟踪翻译块与 IR1 执行；解析器只读两位，不是帮助文本所暗示的三位。 |
| `LATX_DISASSEMBLE_TRACE_CMP` / `-latx-disassemble-trace-cmp <值>` | 未设置；解码器:解码器 | 有 | 比较 lacapstone、nextcapstone、lazydis 的反汇编；冒号后可留空，只选择第一解码器。 |

## 通用运行与日志选项

| 环境变量 / 命令行 | 默认值；取值 | 配置 | 作用与限制 |
| --- | --- | --- | --- |
| `LAT_GDB` / `-g <值>` | 未启用；端口 | 调试构建 | 启动 guest 前等待 GDB 远程连接。 |
| `LAT_STACK_SIZE` / `-s <值>` | 初始实际栈 128 MiB、可见栈 8 MiB；字节数 | 调试构建 | 请求设置 guest 栈大小，解析器包含 k/K/M 后缀分支；启动时可见栈还可能取主机 RLIMIT_STACK。当前处理函数把 qemu_strtoul 返回状态赋给大小，显式指定可能进入 usage 退出，不能保证该入口可用。 |
| `LAT_CPU` / `-cpu <值>` | 目标默认模型；模型名 | 调试构建 | 选择 guest CPU；用 -cpu help 查看当前构建支持的名称。 |
| `LAT_SET_ENV` / `-E <值>` | 继承环境；变量=值列表 | 调试构建 | 给 guest 设置环境变量；与给翻译器设置 LATX_* 选项不同。 |
| `LAT_UNSET_ENV` / `-U <值>` | 不移除；变量名列表 | 调试构建 | 从 guest 环境中移除指定变量。 |
| `LAT_ARGV0` / `-0 <值>` | 原 argv[0]；字符串 | 调试构建 | 覆盖 guest 看到的 argv[0]。 |
| `LAT_UNAME` / `-r <值>` | 默认 release；字符串 | 调试构建 | 覆盖 guest uname 的 release 字符串。 |
| `LAT_GUEST_BASE` / `-B <值>` | 自动；地址 | 调试构建 | 设置 guest 地址到主机地址的偏移。 |
| `LAT_RESERVED_VA` / `-R <值>` | 由目标地址空间决定；大小 | 调试构建 | 预留 guest 虚拟地址空间；支持 k/K/M/G 后缀。 |
| `LAT_LOG` / `-d <值>` | 关闭；类别列表 | 调试构建 | 启用日志类别；-d help 查询类别，不等于打开所有 LATX 调试功能。 |
| `LAT_IMM_SKIP_PC` / `-imm-skip <值>` | 0；十六进制地址 | 调试构建 | 让立即数寄存器优化跳过指定 guest PC。 |
| `LAT_DFILTER` / `-dfilter <值>` | 无过滤；地址范围列表 | 调试构建 | 限制输出日志的地址范围。 |
| `LAT_LOG_FILENAME` / `-D <值>` | stderr；路径 | 调试构建 | 指定日志输出文件。 |
| `LAT_PAGESIZE` / `-p <值>` | 实际主机页大小；字节数 | 调试构建 | 覆盖模拟器使用的页大小设置；不能据此认为内核实际页大小改变。 |
| `LAT_SINGLESTEP` / `-singlestep` | 关闭；无参数 | 调试构建 | 请求单步执行模式。 |
| `LAT_STRACE` / `-strace` | 关闭；无参数 | 调试构建 | 输出 guest 系统调用；不等于 host strace，不包含翻译器的全部主机系统调用。 |
| `LAT_STRACE_ERROR` / `-strace-error` | 关闭；无参数 | 调试构建 | 仅输出失败的 guest 系统调用。 |
| `LAT_RAND_SEED` / `-seed <值>` | 未指定；种子 | 调试构建 | 设置伪随机数种子。 |
| `LAT_TRACE` / `-trace <值>` | 未启用；跟踪配置 | 调试构建 | QEMU 跟踪事件配置：[[enable=]pattern][,events=file][,file=file]。 |
| `LAT_PLUGIN` / `-plugin <值>` | 未装载；插件配置 | 调试构建 | 加载插件：[file=]file[,arg=string]；要求 CONFIG_PLUGIN。 |
| `LAT_LD_PREFIX` / `-L <值>` | ABI 对应运行时目录；路径 | 有 | 指定 guest 动态链接器和库的根目录；用 -runtime-info 查询实际目录和来源。 |
| `LAT_VERSION` / `-version` | 不退出；无参数 | 调试构建 | 显示版本并退出。 |

## 无需 guest 的查询命令

`-h` / `-help` 显示当前二进制的选项；`-version` 显示版本；
`-runtime-info` 显示运行时目录及其来源；`-kzt-libs` 列出库族后退出，
最后一项仅 KZT 构建提供。这些命令没有必须配置的环境变量。

## SOFTFPU_FAST 位定义

位掩码是把每个功能分配到一个二进制位，再用按位或组合的整数。
例如 `0x14000` 选择 FRNDINT 和 FSQRT；仅模式 2 中相关实现使用这些位。
快速路径可能使用主机浮点运算，并保留部分异常回退；不能据此声称覆盖全部
x87 精度、异常、C1 状态或所有操作数形式。

| 位 | 掩码 | 指令 |
| --- | --- | --- |
| 0 | `0x1` | FADDP |
| 1 | `0x2` | FADD |
| 2 | `0x4` | FDIV |
| 3 | `0x8` | FDIVP |
| 4 | `0x10` | FDIVR |
| 5 | `0x20` | FDIVRP |
| 6 | `0x40` | FIADD |
| 7 | `0x80` | FIDIV |
| 8 | `0x100` | FIDIVR |
| 9 | `0x200` | FIMUL |
| 10 | `0x400` | FISUB |
| 11 | `0x800` | FISUBR |
| 12 | `0x1000` | FMUL |
| 13 | `0x2000` | FMULP |
| 14 | `0x4000` | FRNDINT |
| 15 | `0x8000` | FSCALE |
| 16 | `0x10000` | FSQRT |
| 17 | `0x20000` | FSUB |
| 18 | `0x40000` | FSUBP |
| 19 | `0x80000` | FSUBR |
| 20 | `0x100000` | FSUBRP |
| 21 | `0x200000` | FIST |
| 22 | `0x400000` | FISTP |
| 23 | `0x800000` | FST |

## INSTPTN 位定义

此选项控制匹配多条 guest 指令并合并翻译的模式。下表数值是用户应传的掩码，
由 `INSTPTN_OPC_* >> 4` 得到，不可直接复制头文件的 OPC 编号。
`XX` 表示模式允许的中间指令，`XXCC` 表示条件结果使用者；实际匹配还受
寄存器、标志和具体实现限制。传统翻译与 Quicktrans 对同一位的覆盖不一定相同。

| 掩码 | 模式 |
| --- | --- |
| `0x1` | `CMP_JCC` |
| `0x2` | `TEST_JCC` |
| `0x4` | `BT_JCC` |
| `0x8` | `CQO_IDIV` |
| `0x10` | `CMP_SBB` |
| `0x20` | `COMISD_JCC` |
| `0x40` | `COMISS_JCC` |
| `0x80` | `UCOMISD_JCC` |
| `0x100` | `UCOMISS_JCC` |
| `0x200` | `XOR_DIV` |
| `0x400` | `CDQ_IDIV` |
| `0x800` | `CMP_XX_JCC` |
| `0x1000` | `TEST_XX_JCC` |
| `0x2000` | `BT_XX_JCC` |
| `0x4000` | `COMISD_XX_JCC` |
| `0x8000` | `COMISS_XX_JCC` |
| `0x10000` | `UCOMISD_XX_JCC` |
| `0x20000` | `UCOMISS_XX_JCC` |
| `0x40000` | `CMP_XXCC` |
| `0x80000` | `TEST_XXCC` |
| `0x100000` | `UCOMISD_SETA` |
| `0x200000` | `SUB_JCC` |
| `0x400000` | `MOVAPS_VST_X4` |
| `0x800000` | `NEG_CMOVCC` |
| `0x1000000` | `SHR_JCC` |
| `0x2000000` | `AND_JCC` |

## Quicktrans 路径选择与 AOT 示例

以下三个选项在 `latx-config.c` 中直接读取环境，不能使用配置文件或命令行
同名参数；要求 Quicktrans x86_64 构建。首次查询后会缓存，不支持运行中修改。

* `LATX_FAST_TRANSLATOR`：默认 0，设置 1 请求 Quicktrans。
* `LATX_FAST_TRANSLATOR_STRICT`：默认 1，严格模式禁止翻译失败后回退旧路径。
* `LATX_FAST_TRANSLATOR_TRACE_MISSES`：默认 0，输出 Quicktrans 未处理的原因。

三者均将空字符串、0、false、off 视为关闭（后两者不区分大小写）。
其他非空字符串视为开启，因此建议只使用 0/1。

```bash
# 明确选择 Quicktrans，不读取或生成普通 AOT
LATX_FAST_TRANSLATOR=1 LATX_FAST_TRANSLATOR_STRICT=1 \
  LATX_AOT=0 LATX_LOAD_AOT=0 LATX_AOT_GENERATE=0 \
  latx-x86_64 /path/to/program

# 同一 Quicktrans 构建选择传统路径，并只读取已有 AOT
LATX_FAST_TRANSLATOR=0 LATX_AOT=1 LATX_LOAD_AOT=1 \
  LATX_AOT_GENERATE=0 latx-x86_64 /path/to/program
```

`LATX_QUICKTRANS_AOT_COLLECT=1` 是独立实验功能：要求严格 Quicktrans、硬浮点、
关闭运行时 AOT 装载，并禁用 Wine/KZT。收集沿用旧路径的 AOT 退出处理入口：
正常最终退出、已有的主线程退出入口，以及默认致命信号终止（包括 SIGTERM、
SIGABRT 和 SIGSEGV）。已有 kill/tgkill 入口也会在 guest 向自身发送 SIGKILL 前
尝试保存快照；外部 SIGKILL 无法拦截。忽略信号或安装了应用处理函数的情况仍按
应用原有方式处理，绕过 AOT 退出入口的异常退出不保证收集。

未使用控制器时，退出过程等待生成进程；使用 `latx-aot-collect` 控制器时，
先保存快照，再由控制器在应用进程树退出后放行生成工作。保存快照或生成失败
不会把信号退出改成成功退出，也不保证每个进程都成功生成缓存。运行期间的
翻译块清空不会触发首次收集；不支持的覆盖来源和失败条件见下面的专项文档。
生成文件不表示下次确实加载，也不证明混合执行正确性。完整限制与缓存隔离示例见
[Quicktrans 退出时生成 AOT](../latx/quicktrans-aot-collect.md)。

## 没有公开入口的内部 option

C 变量名不是环境变量名。以下变量没有在当前命令行/配置注册表中提供独立入口，
不要自行构造 LATX_* 名称：

* `option_enable_lasx`：初始化为 1，具体使用还受主机和翻译路径限制。
* `option_fast_atomic`：由 `have_am()` 检测主机原子访存支持后设为 1 或 0。
* `option_flag_reduction`：相应编译条件下初值为 1，控制标志计算消除。
* `option_split_tb`：CONFIG_LATX_SPLIT_TB 下初始化为 1。
* `option_jr_ra_stack`：JRRA_STACK 与 LSFPU 同时编译时为 1，否则为 0。
* `option_tunnel_lib`：CONFIG_LATX_TUNNEL_LIB 下为 1，否则为 0；
  LATX_OPTIMIZE=tunnel-lib 只能请求启用。
* `option_shadow_file`：LOW_MEM_MODE_0 下初始化为 1，否则为 0。
* `option_lative`：初值 0，现有特殊参数处理会对 basename 为 program 的目标设为 1。

## 维护与核对

增加选项时同时核对：`linux-user/main.c` 的注册和处理函数、
`target/i386/latx/include/latx-options.h` 的配置入口、
`target/i386/latx/latx-options.c` 的初值，以及实际使用变量的翻译或运行代码。
仅看到变量声明不足以确认选项可设置或会生效。

本参考经过源码核对；没有逐一运行所有取值和组合，也不表示完成应用兼容性或性能验证。
