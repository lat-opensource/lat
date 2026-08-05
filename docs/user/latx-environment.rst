LATX 配置参考
=============

LATX 的默认配置适合先验证程序能否运行。只有在准备 guest 运行时、排查兼容性
问题或收集调试信息时，才建议修改配置；每次只改一个选项，便于判断实际影响。

配置在 guest 启动时读取。运行中的 guest 不会因为配置文件或环境变量变化而
热切换行为。


快速用法
========

仅为一次运行设置选项：

.. code-block:: bash

    LATX_SOFTFPU=1 latx-x86_64 /path/to/x86_64-program

使用命令行参数得到相同效果：

.. code-block:: bash

    latx-x86_64 -latx-softfpu 1 /path/to/x86_64-program

为某个 guest 持久设置选项，可以把配置写入用户配置文件：

.. code-block:: ini

    [guest_program]
    LATX_SOFTFPU = 1

节名使用 guest 可执行文件的 basename，区分大小写。例如
``/opt/example/guest_program`` 对应 ``[guest_program]``。


配置优先级
==========

同一个选项出现在多个位置时，从高到低依次为：

1. 命令行参数。
2. 环境变量。
3. 用户配置文件 ``~/.config/latx-*.conf``。
4. 系统配置文件 ``/etc/latx-*.conf``。
5. 程序内置默认值。

高优先级设置会覆盖低优先级设置。为了避免难以追踪的组合，普通用户优先使用
一次性环境变量，确认有效后再写入配置文件。


配置文件
========

LATX 根据 guest 位数读取不同文件：

.. list-table::
   :header-rows: 1

   * - Guest
     - 系统配置
     - 用户配置
   * - i386
     - ``/etc/latx-i386.conf``
     - ``~/.config/latx-i386.conf``
   * - x86_64
     - ``/etc/latx-x86_64.conf``
     - ``~/.config/latx-x86_64.conf``

配置文件模板位于代码库的 ``configs/`` 目录。可以从模板创建用户配置：

.. code-block:: bash

    mkdir -p ~/.config
    cp configs/latx-x86_64.conf ~/.config/

文件中第一个 section 之前的键对所有 guest 生效，section 内的键仅对对应程序
生效：

.. code-block:: ini

    # 全局设置必须放在第一个 section 之前
    LATX_AOT = 0

    [guest_program]
    LATX_SOFTFPU = 1

    [another-program]
    LATX_ANONYM = 1

Meson 安装目标会把模板安装到 configure 指定的 ``sysconfdir``。默认值是
``/usr/local/etc``，但 LATX 运行时读取 ``/etc/latx-*.conf``；需要系统配置
直接生效时，请使用 ``--sysconfdir=/etc`` 配置构建。

``build-release.sh`` 生成的 ``tar.xz`` 包不包含配置模板，因此安装发布包不会
覆盖现有的 LATX 配置文件。


常用选项
========

下表以当前 O1 release 构建的默认行为为准。只有编译进翻译器的功能才接受对应
选项；配置文件包含不支持的选项时，LATX 会报告 ``no option``。

.. list-table::
   :header-rows: 1
   :widths: 22 12 66

   * - 选项
     - 默认值
     - 用途与注意事项
   * - ``LATX_AOT``
     - ``1``
     - 控制 AOT 预翻译。设为 ``0`` 可在排查缓存或翻译差异时关闭 AOT。软浮点、
       隐藏虚拟化和部分调试模式会自动关闭 AOT。
   * - ``LATX_KZT``
     - ``0``
     - 控制 64 位构建中的库直通。``1`` 表示按兼容性检查启用，``2`` 是供测试
       使用的强制模式。32 位构建不提供该选项。
   * - ``LATX_SOFTFPU``
     - ``0``
     - 浮点结果异常时可尝试 ``1``；``2`` 是另一种软浮点模式。非零值会自动
       关闭 AOT 和 rounding 优化，通常会降低性能。
   * - ``LATX_ANONYM``
     - ``0``
     - 设为 ``1``，对会检测虚拟化环境的 guest 隐藏部分 LATX 特征。启用时会
       自动关闭 AOT。
   * - ``LATX_SMC``
     - ``0``
     - 选择自修改代码处理策略。``0`` 按页失效，``1`` 按 TB 失效，``2`` 额外
       启用共享内存监控，``6`` 再启用 store helper。它不是简单的开关。
   * - ``LATX_AVX_CPUID``
     - ``1``
     - 仅 AVX 构建可用。设为 ``0`` 会改变 AVX 相关 CPUID 上报，但不会关闭
       已编译的 AVX 翻译；必须在 guest 启动前设置。
   * - ``LAT_LD_PREFIX``
     - ABI 路径
     - 覆盖动态链接 guest 的运行时根目录，等价于 ``-L <path>``。先用
       ``-runtime-info`` 确认当前选择，避免掩盖运行时安装问题。


指定 guest 运行时
==================

查看 64 位或 32 位翻译器当前选择的运行时：

.. code-block:: bash

    latx-x86_64 -runtime-info
    latx-i386 -runtime-info

输出中的 ``runtime_root`` 是动态链接器和 guest 库的查找根目录，
``runtime_source`` 说明该值来自内置默认、配置文件、环境变量还是命令行。

临时使用自定义运行时：

.. code-block:: bash

    LAT_LD_PREFIX=/path/to/x86_64-root \
        latx-x86_64 /path/to/x86_64-program

也可以使用命令行参数：

.. code-block:: bash

    latx-x86_64 -L /path/to/x86_64-root /path/to/x86_64-program

ABI 1.0 的默认目录是 ``/usr/gnemul/latx-i386`` 和
``/usr/gnemul/latx-x86_64``；ABI 2.0 的默认目录是
``/usr/gnemul/lat-i386`` 和 ``/usr/gnemul/lat-x86_64``。


调试选项
========

``latxbuild/build32-dbg.sh`` 和 ``latxbuild/build64-dbg.sh`` 生成的 debug
翻译器提供额外日志参数。常用入口包括：

.. list-table::
   :header-rows: 1

   * - 命令行
     - 环境变量
     - 用途
   * - ``-strace``
     - ``LAT_STRACE``
     - 记录 guest 系统调用。
   * - ``-strace-error``
     - ``LAT_STRACE_ERROR``
     - 仅记录失败的 guest 系统调用。
   * - ``-d <items>``
     - ``LAT_LOG``
     - 启用指定 QEMU/LATX 日志类别。
   * - ``-D <file>``
     - ``LAT_LOG_FILENAME``
     - 把日志写入文件，默认输出到 stderr。
   * - ``-g <port>``
     - ``LAT_GDB``
     - 启动后等待 guest GDB 远程连接。

低层优化和调试参数会随实现变化。完整注册表以
``linux-user/main.c`` 中的 ``arg_table`` 为准，默认值以
``target/i386/latx/latx-options.c`` 中的 ``options_init()`` 为准。问题定位流程
请参阅 `调试与问题定位指南`_。


.. _调试与问题定位指南: https://github.com/lat-opensource/lat/wiki/%E8%B0%83%E8%AF%95%E4%B8%8E%E9%97%AE%E9%A2%98%E5%AE%9A%E4%BD%8D%E6%8C%87%E5%8D%97
