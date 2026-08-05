===========
LATX 配置信息
===========


LATX 提供了配置文件、环境变量、命令行参数等多种方法，控制LATX运行时的行为、优化策略和调试功能。

配置项的优先级为 命令行 > 环境变量 > ``~/.config/latx-*.conf`` > ``/etc/latx-*.conf``。


配置文件
========


LATX 的配置文件分为 32 位和 64 位版本。系统配置文件分别为
``/etc/latx-i386.conf`` 和 ``/etc/latx-x86_64.conf``；用户配置文件分别为
``~/.config/latx-i386.conf`` 和 ``~/.config/latx-x86_64.conf``。这些文件均
采用 ``.ini`` 格式。


配置文件模板位于代码库的 ``configs/`` 目录。Meson 安装目标会把模板安装到
configure 指定的 ``sysconfdir``；默认值是 ``/usr/local/etc``，而 LATX 运行时
读取 ``/etc/latx-*.conf``。若要让安装的系统配置直接生效，配置构建时需要指定
``--sysconfdir=/etc``。当前 ``build-release.sh`` 生成的 ``tar.xz`` 包不包含这些
模板。运行时会根据 guest 程序的位数选择对应配置文件。


您可以将已安装的系统配置文件或代码库中的模板复制到 ``~/.config/`` 目录，
然后添加自己的配置信息。

.. code-block:: ini


    LATX_XXX=X # 对所有 guest 生效的总配置


    [guest_program] # 对guest_program生效的配置
    LATX_YYY=Y


配置流程
========


LATX 的配置的逐级流程如下：


.. code-block:: text


    main()
    ├── options_init()          # 初始化默认值
    ├── conf_init()             # 加载配置文件
    │    ├── /etc/latx-x86_64.conf 或 /etc/latx-i386.conf
    │    └── ~/.config/latx-x86_64.conf 或 ~/.config/latx-i386.conf
    ├── 读取环境变量
    ├── 解析命令行参数
    └── handle_arg_latx_*       # 写入 option_* 变量


所有默认值在 ``latx-options.c`` 中的 ``options_init()`` 中统一设置，这些默认值决定了 LATX 在无任何配置时的行为。


随后逐级加载配置信息，冲突的配置由高优先覆盖低优先级。


参数注册与入口
==============


所有支持的参数（包括环境变量和命令行）统一定义在：


.. code-block:: text


    linux-user/main.c → struct qemu_argument arg_table


每个参数包含：


.. code-block:: c


    struct qemu_argument {
        const char *argv; #命令行名称
        const char *env; #环境变量
        bool has_arg; #是否带参数
        void (*handle_opt)(const char *arg); #处理函数
        const char *example;
        const char *help; #help信息
    };


参数设置方式
============


LATX 支持三种方式设置运行参数（以软浮点功能为例）：


1.  配置文件\
    ``[guest_program]``\
    ``LATX_SOFTFPU = 1``
2.  环境变量\
    ``export LATX_SOFTFPU=1``
3.  命令行\
    ``latx-x86_64 -latx-softfpu 1 guest_program``


三种方式最终都会调用同一处理函数：


.. code-block:: text


    handle_arg_latx_softfpu


配置介绍
========


配置参数可分为优化类和调试类，部分配置不支持环境变量和命令行设置，仅支持初始化或编译信息设置。


AOT（预翻译）会保存和恢复预翻译信息。部分配置与 AOT 冲突，启用这些配置时，
对应的参数处理函数会自动关闭 AOT。


以下未特殊说明的均为置 1 时开启。


优化类配置参数
--------------


.. list-table::
   :header-rows: 1
   :widths: auto


   * - 环境变量
     - 命令行
     - 对应代码变量
     - 功能
     - AOT关系
     - 备注
   * - LATX\_OPTIMIZE
     - ``-latx-optimize tunnel-lib``
     - option\_tunnel\_lib
     - 影响reg\_priv\_plt执行
     - 
     - 
   * - LATX\_SMC
     - ``-latx-smc 0/1/2/6``
     - option\_smc\_opt
     - 自修改代码优化
     - 
     - 
   * - LATX\_CLOSE\_PARALLEL
     - ``-latx-close-parallel``
     - close\_latx\_parallel
     - 关闭并行
     - 
     - 
   * - LATX\_SOFTFPU
     - ``-latx-softfpu 1/2``
     - option\_softfpu
     - 软件浮点
     - 冲突
     - 关闭AOT
   * - LATX\_SOFTFPU\_FAST
     - ``-latx-softfpu-fast 0xffffff``
     - option\_softfpu\_fast
     - 位控制的软浮点进阶优化
     - 冲突
     - 仅 softfpu=2 有效
   * - LATX\_PRLIMIT
     - ``-latx-prlimit``
     - option\_prlimit
     - 资源限制
     - 
     - 
   * - LATX\_ROUNDING\_OPT
     - ``-latx-rounding``
     - option\_set\_rounding\_opt
     - 浮点优化
     - 冲突
     - 与 softfpu 冲突
   * - LATX\_CVT\_OPT
     - ``-latx-cvt-opt``
     - option\_cvt\_opt
     - 转换优化
     - 
     - 
   * - LATX\_AVX\_CPUID
     - ``-latx-avx-cpuid``
     - option\_avx\_cpuid
     - 控制 AVX 相关 CPUID 上报
     - 
     - 仅在启用 AVX 指令翻译支持的构建中可用，默认值为 1；设为 0 会隐藏
       AVX 相关 CPUID，但不会禁用 AVX 指令翻译。必须在 guest 启动前设置，
       运行期间不能切换
   * - LATX\_KZT
     - ``-latx-kzt``
     - option\_kzt
     - 库直通
     - 
     - 
   * - LATX\_FPUTAG
     - ``-latx-fputag``
     - option\_fputag
     - 使用软件 FPU tag
     - 
     - 
   * - SAVE\_XMM
     - ``-save-xmm``
     - option\_save\_xmm
     - xmm 保存控制
     - 
     - 
   * - LATX\_JRRA
     - ``-latx-jrra``
     - | option\_jr\_ra
       | option\_jr\_ra\_stack
     - jr ra 优化
     - 冲突
     - 
   * - LATX\_IMM\_REG
     - ``-latx-imm-reg 1111``
     - | option\_imm\_reg
       | option\_imm\_rip
       | option\_imm\_complex
       | option\_imm\_precache
     - imm 优化
     - 
     - 位域控制
   * - LATX\_MT
     - ``-latx-mem-test``
     - option\_mem\_test
     - 内存权限检测
     - 冲突
     - 
   * - LATX\_REAL\_MAPS
     - ``-latx-real-maps``
     - option\_real\_maps
     - 使用真实 maps
     - 冲突
     - 
   * - LATX\_MONITOR\_SHARED\_MEM
     - ``-latx-monitor-shared-mem``
     - option\_monitor\_shared\_mem
     - 共享内存监控
     - 
     - 
   * - LATX\_AOT
     - ``-latx-aot``
     - | option\_aot
       | option\_load\_aot
       | option\_aot\_wine
     - 预翻译
     - 
     - 部分优化下关闭
   * - LAT\_AOT\_FILE\_SIZE
     - ``-latx-aot-file-size file``
     - aot\_file\_size\_optarg
     - 
     - 
     - 
   * - LAT\_AOT\_LEFT\_FILE\_SIZE
     - ``-latx-aot-left-file-size``
     - aot\_left\_file\_minsize\_optarg
     - 预翻译相关
     - 
     - 
   * - LATX\_AOT\_WINE\_PEFILES\_CACHE
     - ``-latx-aot-wine-pefiles-cache``
     - latx\_aot\_wine\_pefiles\_cache
     - 
     - 
     - 
   * - LATX\_ANONYM
     - ``-latx-anonym``
     - option\_anonym
     - 隐藏虚拟化
     - 冲突
     - 
   * - LATX\_MMAP\_START
     - ``-latx-mmap_start addr``
     - mmap\_next\_start
     - 设置mmap初始地址
     - 
     - 
   * - LATX\_MMAP\_FIXED
     - ``-latx-mmap_fixed abi_ulong``
     - option\_mmap\_fixed
     - 设置强制强制使用 MAP\_FIXED 的 mmap
     - 
     - 
   * - LATX\_UNIMP\_DUMP
     - ``-latx-unimp-dump``
     - option\_mmap\_fixed
     - 打印不支持的 syscall
     - 
     - 


部分无环境变量和命令行控制的优化选项可在 ``optimize-config.h`` 修改编译信息开启或关闭


.. list-table::
   :header-rows: 1
   :widths: auto


   * - 对应宏
     - 对应代码变量
     - 功能
   * - CONFIG\_LATX\_FLAG\_REDUCTION
     - option\_flag\_reduction
     - flag 优化
   * - CONFIG\_LATX\_INSTS\_PATTERN
     - option\_instptn
     - inst pattern 优化
   * - CONFIG\_LATX\_SPLIT\_TB
     - option\_split\_tb
     - TB分离
   * - 自动检测
     - option\_enable\_lasx
     - LASX 启用
   * - 自动检测
     - option\_fast\_atomic
     - 原子优化
   * - LOW\_MEM\_MODE\_0
     - option\_shadow\_file
     - shadow 页
   * - 无
     - option\_lative
     - 随机测试用
   * - CONFIG\_LATX\_AOT
     - option\_load\_aot
     - AOT 加载控制
   * - 无
     - option\_aot\_wine
     - AOT wine


调试类配置参数
--------------


调试类配置需在 debug 模式的 LATX 下使用，添加编译参数 ``--enable-debug`` 或使用 debug 模式编译脚本 ``latxbuild/build64-dbg.sh``


下表总结了 debug 模式下可开启的调试信息开关：


.. list-table::
   :header-rows: 1
   :widths: auto


   * - 环境变量
     - 命令行
     - 对应代码变量
     - 功能
   * - LATX\_BEGIN\_TRACE
     - ``-latx-runtime-trace-begin``
     - option\_begin\_trace\_addr
     - trace 起点
   * - LATX\_END\_TRACE
     - ``-latx-runtime-trace-end``
     - option\_end\_trace\_addr
     - trace 终点
   * - LATX\_DUMP
     - ``-latx-dump 11111``
     - | option\_dump
       | option\_dump\_ir1
       | option\_dump\_ir2
       | option\_dump\_host
       | option\_dump\_profile
     - dump 信息
   * - LATX\_SHOW\_TB
     - ``-latx-show-tb``
     - debug\_tb\_pc
     - 打印指定 TB
   * - LATX\_TRACE\_MEM
     - ``-latx-trace-mem``
     - latx\_trace\_mem
     - 内存写追踪
   * - LATX\_BREAK\_INSN
     - ``-latx-break-insn``
     - latx\_break\_insn
     - 指令断点
   * - LATX\_DEBUG\_LATIVE
     - ``-latx-debug-lative``
     - option\_debug\_lative
     - 调试模式
   * - LATX\_ENABLE\_FCSR\_EXC
     - ``-latx-enable-fcsr-exc``
     - option\_enable\_fcsr\_exc
     - 浮点异常
   * - LATX\_UNLINK
     - ``-latx-unlink``
     - | latx\_unlink\_count
       | latx\_unlink\_cpu
     - TB unlink 控制
   * - LATX\_TRACE
     - ``-latx-trace 11``
     - | option\_trace\_tb
       | option\_trace\_ir1
     - 打印 TB 信息
   * - LATX\_DISASSEMBLE\_TRACE\_CMP
     - ``-latx-disassemble-trace-cmp a:b``
     - option\_latx\_disassemble\_trace\_cmp
     - 对比不同反汇编器结果


.. list-table::
   :header-rows: 1
   :widths: auto


   * - 环境变量
     - 命令行
     - 功能说明
   * - 无
     - ``-h or help``
     - 打印帮助信息
   * - LAT\_GDB
     - ``-g <port>``
     - 启动时暂停执行，并在指定端口等待 GDB 远程连接（用于调试 guest 程序）
   * - LAT\_STACK\_SIZE
     - ``-s <size>``
     - 设置 guest 程序的栈大小（单位：字节）
   * - LAT\_CPU
     - ``-cpu <model>``
     - 指定模拟 CPU 类型（影响指令集与特性支持）
   * - LAT\_SET\_ENV
     - ``-E var=value``
     - 为 guest 程序设置环境变量
   * - LAT\_UNSET\_ENV
     - ``-U var``
     - 从 guest 环境中移除指定变量
   * - LAT\_ARGV0
     - ``-0 <argv0>``
     - 强制指定 guest 程序的 argv[0]
   * - LAT\_UNAME
     - ``-r <string>``
     - 修改 uname 返回值（用于绕过兼容性检查）
   * - LAT\_GUEST\_BASE
     - ``-B <addr>``
     - 设置 guest 内存基地址（用于地址空间布局控制）
   * - LAT\_RESERVED\_VA
     - ``-R <size>``
     - 预留一段虚拟地址空间给 guest 使用
   * - LAT\_LOG
     - ``-d <items>``
     - 启用调试日志（如 cpu、exec、in\_asm 等）
   * - LAT\_IMM\_SKIP\_PC
     - ``-imm-skip``
     - imm\_re优化跳过的pc
   * - LAT\_DFILTER
     - ``-dfilter``
     - 基于地址范围过滤日志记录
   * - LAT\_LOG\_FILENAME
     - ``-D <file>``
     - 指定日志输出文件（默认 stderr）
   * - LAT\_PAGESIZE
     - ``-p <size>``
     - 强制指定 host 页大小
   * - LAT\_SINGLESTEP
     - ``-singlestep``
     - 启用单步执行模式（每条指令执行后返回）
   * - LAT\_STRACE
     - ``-strace``
     - 跟踪 guest 的系统调用
   * - LAT\_STRACE\_ERROR
     - ``-strace-error``
     - 仅打印失败的系统调用
   * - LAT\_RAND\_SEED
     - ``-seed <value>``
     - 设置随机数种子（用于可重复测试）
   * - LAT\_TRACE
     - ``-trace``
     - 启用 trace 事件系统
   * - LAT\_PLUGIN
     - ``-plugin <file>``
     - 加载插件（用于扩展功能或分析）
   * - LAT\_LD\_PREFIX
     - ``-L <path>``
     - 指定 ELF loader 路径（用于自定义运行环境）
