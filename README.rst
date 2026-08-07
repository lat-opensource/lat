================================================
LATX (LoongArch Architecture Translator for x86)
================================================

.. image:: https://img.shields.io/badge/license-GPL--2.0-blue.svg
   :target: COPYING
   :alt: GPL-2.0 license

.. image:: https://github.com/lat-opensource/lat/actions/workflows/tests.yml/badge.svg
   :target: https://github.com/lat-opensource/lat/actions/workflows/tests.yml
   :alt: LAT Tests

LATX（LoongArch Architecture Translator for x86）即龙芯 x86 架构转译器，是
一个面向 LoongArch 架构的高性能用户级二进制翻译器，用于在龙架构系
统上高效地运行 32/64 位 x86 应用程序。

LATX 基于 QEMU 6 版本开发并进行了深度优化，性能相比原生 QEMU 有显著提升。
项目利用龙架构的各指令集扩展（如向量扩展和二进制转译指令集）对 X86 指令集
进行了高效翻译，并采用了AOT（Ahead-of-Time ）预编译、运行时库直通等关键优
化技术，其中库直通优化思想借鉴及引用了 box64 项目的部分源码。

`See the English Version Here >> <README.en.rst>`_


项目背景
========

在 LoongArch 架构生态建设过程中，运行已有的 x86 程序存在兼容性和性能瓶颈，
原生 QEMU 等模拟器在性能和兼容性上并不能完全满足需求。因此，我们在 QEMU 6
的基础上进行了二次开发，通过引入预编译、库直通以及其他针对性优化，大幅减少
了指令翻译和执行的开销，努力实现“更快、更稳定、更兼容”的目标。


使用前提
========

- 本文的构建流程需要在 LoongArch Linux 环境中执行，生成的翻译器也运行在
  LoongArch Linux 上。
- LATX 运行需要主机 CPU 和内核提供 LSX 与 LBT_X86 扩展。普通 SIMD 翻译也会
  在主机支持时使用 LASX；启动时未检测到 LASX，LATX 会关闭 LASX 路径并改用
  128 位 LSX 指令翻译。
- LATX 支持 LoongArch ABI 1.0 和 ABI 2.0。构建配置会自动检测当前环境的 ABI，
  因此请在与安装目标 ABI 一致的环境中构建。
- x86 Linux 程序可直接通过 LATX 运行；运行 x86 Windows 程序还需要 x86 Wine
  及与 Wine 匹配的运行时环境。


快速开始
========

在 LoongArch Linux 主机上安装 `编译依赖`_ 后，执行：

.. code-block:: bash

    git clone --depth=1 --recursive https://github.com/lat-opensource/lat
    cd lat
    ./latxbuild/build-release.sh

脚本会生成 ``lat-<版本>-<日期>.tar.xz``。安装最新生成的包，并让 binfmt 和
sysctl 配置立即生效：

.. code-block:: bash

    package=$(ls -1t lat-*.tar.xz | head -n 1)
    sudo tar -Jxf "$package" -C / --strip-components=1
    sudo systemctl restart systemd-binfmt.service
    sudo systemctl restart systemd-sysctl.service

也可以重启系统使配置生效。确认两个翻译器已经安装：

.. code-block:: bash

    latu-runtime-manager status

第一次验证建议使用静态链接的 x86_64 Linux 程序，因为它不依赖额外的 x86
运行时：

.. code-block:: bash

    wget -O busybox.pkg.tar.zst \
        https://archlinux.org/packages/extra/x86_64/busybox/download/
    tar xf busybox.pkg.tar.zst
    latx-x86_64 ./usr/bin/busybox uname -m

注册 binfmt 后也可以直接执行 ``./usr/bin/busybox uname -m``。动态链接程序还
需要在 ``/usr/gnemul`` 下准备与程序匹配的 x86 运行时，详见
`编译、安装和运行 Wiki`_。


编译依赖
========

选择当前发行版对应的命令。依赖列表也包含首次 BusyBox 验证使用的 ``file``、
``wget`` 和 ``zstd``。项目优先使用版本不低于 0.55.3 的系统 Meson；系统 Meson
不可用或版本过低时，会使用 ``--recursive`` 获取的 Meson 子模块。Python 版本
必须不低于 3.6。

Debian / Ubuntu：

.. code-block:: bash

    sudo apt install -y git meson ninja-build libssl-dev libc6 gcc g++ \
        pkg-config libglib2.0-dev libdrm-dev lsb-release make python3 \
        python3-setuptools binutils file tar wget xz-utils zstd

Arch Linux：

.. code-block:: bash

    sudo pacman -S --needed git make meson ninja gcc pkgconf glib2 python \
        python-setuptools openssl binutils file tar wget xz zstd

安同 OS（AOSC OS）：

.. code-block:: bash

    sudo oma install -y git make gcc meson nettle pcre2 libffi gnutls glib zlib \
        glib-static libgcrypt-static libgpg-error-static libnfs-static \
        pcre-static zlib-static zstd-static openssl-static pkg-config ninja \
        binutils file tar wget xz zstd

Fedora：

.. code-block:: bash

    sudo dnf install gcc gcc-c++ make git ninja-build meson openssl-devel \
        glib2-devel binutils file tar wget xz zstd

可以在构建前检查基础工具：

.. code-block:: bash

    python3 --version
    ninja --version


构建与产物
==========

默认发布构建同时生成 32 位和 64 位翻译器：

.. code-block:: bash

    ./latxbuild/build-release.sh

生成的 ``tar.xz`` 包包含：

- ``usr/bin/latx-i386``：运行 32 位 i386 程序。
- ``usr/bin/latx-x86_64``：运行 64 位 x86_64 程序。
- ``usr/bin/latu-runtime-manager``：检查翻译器安装状态。
- ``usr/lib/binfmt.d/*.conf``：注册 x86 ELF 的 binfmt 配置。
- ``usr/lib/sysctl.d/mmap_min_addr.conf``：LATX 所需的地址映射配置。

打包时会移除符号表和调试信息，因此包内二进制比 ``build32/`` 和
``build64/`` 中的原始产物更小。安装和日常使用以包内二进制为准；构建目录中
的未剥离文件适用于调试。

运行动态链接程序
================

动态链接的 x86 程序需要匹配的 guest 运行时。可以查看当前翻译器选择的目录：

.. code-block:: bash

    latx-x86_64 -runtime-info
    latx-i386 -runtime-info

默认目录因宿主 ABI 而异。ABI 1.0 使用 ``/usr/gnemul/latx-*``，ABI 2.0 使用
``/usr/gnemul/lat-*``。运行时的安装、Wine 配套关系、升级和卸载说明见
`编译、安装和运行 Wiki`_。


支持范围与已知限制
==================

Guest 程序范围：

- 支持 32 位（i386）和 64 位（x86_64）的 x86 Linux 用户态程序，不提供 x86
  全系统模拟。
- x86 Windows 程序通过 x86 Wine 运行，需要与 Wine 匹配的运行时环境，详见
  `编译、安装和运行 Wiki`_。
- 16 位实模式（vm86）程序不在支持范围内，未做验证。

x86 指令集覆盖：

- 发布构建（O1 档）翻译并上报 x87、MMX、SSE、SSE2、SSE3，以及 SSSE3、
  SSE4.1、SSE4.2、POPCNT、AES、PCLMULQDQ 和 CMPXCHG16B。
- AVX、AVX2、FMA、F16C、BMI1/BMI2 属于测试档优化（-O 3），默认不启用，
  发布包不包含这些指令的翻译。需要时按 `AVX 指令支持`_ 一节自行构建。

运行行为说明：

- 库直通（ ``LATX_KZT`` ）仅在 64 位构建中提供，默认按兼容性检查启用；
  32 位构建没有该选项。
- 多线程 guest 程序沿用 QEMU 用户态多线程模型，原子指令根据宿主内核能力
  选择优化路径。
- 自修改代码默认按页失效。个别程序出现兼容性问题时，可以调整
  ``LATX_SMC`` 策略；浮点结果异常时可以尝试 ``LATX_SOFTFPU`` （会降低性能
  并自动关闭 AOT）。这些选项的语义见
  `LATX 配置信息 <docs/user/latx-environment.rst>`_。


配置
====

LATX 支持系统配置文件、用户配置文件、环境变量和命令行参数。优先级、常用配置
及示例见 `LATX 配置信息 <docs/user/latx-environment.rst>`_。


AVX 指令支持
============

当前 ``build-release.sh`` 不提供 AVX 打包参数。``build32.sh`` 和
``build64.sh`` 均支持 ``-a``；该选项会在配置阶段传入
``--enable-latx-avx-opt``，为当前构建目标启用 x86 AVX 指令翻译支持：

.. code-block:: bash

    ./latxbuild/build32.sh -c -a
    ./latxbuild/build64.sh -c -a

``-c`` 会重新生成构建配置，切换 AVX 支持时不能省略。

AVX 构建默认向 guest 上报相关 CPUID 信息。如需隐藏该信息，必须在启动 guest
前设置：

.. code-block:: bash

    export LATX_AVX_CPUID=0

该设置会改变 CPUID 上报，但不会关闭已经编译的 AVX 指令翻译，也不支持在
guest 运行期间热切换。


文档与问题反馈
==============

- `LAT Wiki <https://github.com/lat-opensource/lat/wiki>`_
- `编译、安装和运行 Wiki`_
- `调试与问题定位指南`_
- `Issues <https://github.com/lat-opensource/lat/issues>`_
- `Discussions <https://github.com/lat-opensource/lat/discussions>`_

提交 Pull Request 前，请阅读 `贡献指南 <CONTRIBUTING.md>`_ 和
`提交规范 <COMMIT_CONVENTION.md>`_。每个提交都必须包含 DCO
``Signed-off-by`` 签署。


贡献者
======

|contrib-luzeng| |contrib-hanlu| |contrib-wenqiang| |contrib-jing| |contrib-qi| |contrib-chaoyi| |contrib-rengan| |contrib-xiaotian| |contrib-haiyong| |contrib-niugenen|

LATX 感谢每一位贡献者。各阶段贡献者与贡献领域说明见
`CONTRIBUTORS.md <CONTRIBUTORS.md>`_，开源后的贡献记录见
`GitHub Contributors <https://github.com/lat-opensource/lat/graphs/contributors>`_。


许可证
======

本项目基于 QEMU 源代码进行二次开发，原始项目遵循 GNU 通用公共许可证第 2 版
（GNU General Public License, version 2，简称 GPLv2）发布。

因此，本项目同样遵循 GPLv2 协议。


致谢
====

特别鸣谢 QEMU 项目与 box64 项目及开发者，他们的开源成果为本项目提供了宝贵
的参考与支持。

------------

如有任何问题或建议，欢迎通过 `Issue <https://github.com/lat-opensource/lat/issues>`_ 与我们交流！


.. _编译、安装和运行 Wiki: https://github.com/lat-opensource/lat/wiki/%E7%BC%96%E8%AF%91%E4%B8%8E%E8%BF%90%E8%A1%8C
.. _调试与问题定位指南: https://github.com/lat-opensource/lat/wiki/%E8%B0%83%E8%AF%95%E4%B8%8E%E9%97%AE%E9%A2%98%E5%AE%9A%E4%BD%8D%E6%8C%87%E5%8D%97

.. |contrib-luzeng| image:: https://github.com/luzeng87.png?size=64
   :target: https://github.com/luzeng87
   :alt: Lu Zeng
.. |contrib-hanlu| image:: https://github.com/LaurenIsACoder.png?size=64
   :target: https://github.com/LaurenIsACoder
   :alt: Hanlu Li
.. |contrib-wenqiang| image:: https://github.com/ganjue66da.png?size=64
   :target: https://github.com/ganjue66da
   :alt: Wenqiang Wei
.. |contrib-jing| image:: https://github.com/JonLeeTaoShan.png?size=64
   :target: https://github.com/JonLeeTaoShan
   :alt: Jing Li
.. |contrib-qi| image:: https://github.com/specialpointcentral.png?size=64
   :target: https://github.com/specialpointcentral
   :alt: Qi Hu
.. |contrib-chaoyi| image:: https://github.com/rmjskhy.png?size=64
   :target: https://github.com/rmjskhy
   :alt: Chaoyi Liu
.. |contrib-rengan| image:: https://github.com/y347812075.png?size=64
   :target: https://github.com/y347812075
   :alt: Rengan Yue
.. |contrib-xiaotian| image:: https://github.com/yetist.png?size=64
   :target: https://github.com/yetist
   :alt: Xiaotian Wu
.. |contrib-haiyong| image:: https://github.com/sunhaiyong1978.png?size=64
   :target: https://github.com/sunhaiyong1978
   :alt: Sun Haiyong
.. |contrib-niugenen| image:: https://github.com/NiuGenen.png?size=64
   :target: https://github.com/NiuGenen
   :alt: NiuGenen
