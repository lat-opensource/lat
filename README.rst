===========
LATX (Loongson Architecture Translator for x86)
===========

LATX（Loongson Architecture Translator for x86）即龙芯 x86 架构转译器，是
一个面向 LoongArch 架构的高性能用户级二进制翻译器，用于在龙芯（龙架构）系
统上高效地运行 32/64 位 x86 应用程序。

LATX 基于 QEMU 6 版本开发并进行了深度优化，性能相比原生 QEMU 有显著提升。
项目利用龙架构的各指令集扩展（如向量扩展和二进制转译指令集）对 X86 指令集
进行了高效翻译，并采用了AOT（Ahead-of-Time ）预编译、运行时库直通等关键优
化技术，其中库直通优化思想借鉴及引用了 box64 项目的部分源码。


`See the English Version Here >> <README.en.rst>`_

提交规范
========

`提交规范（中文） <COMMIT_CONVENTION.md>`_


项目背景
========

在 LoongArch 架构生态建设过程中，运行已有的 x86 程序存在兼容性和性能瓶颈，
原生 QEMU 等模拟器在性能和兼容性上并不能完全满足需求。因此，我们在 QEMU 6
的基础上进行了二次开发，通过引入预编译、库直通以及其他针对性优化，大幅减少
了指令翻译和执行的开销，努力实现“更快、更稳定、更兼容”的目标。

历史演进
========

项目历经多个开发阶段：

- **2021 年**：项目启动，完成 LATX 到 QEMU 6 的移植，Q3 项目进入 Alpha 阶段。
- **2022 年**：支持库直通等优化，Q3 项目进入到 Beta 阶段。
- **2023 年**：持续完善系统调用等接口的支持，以及更细致的指令优化。
- **2024 年**：项目进入到 RC 阶段。


项目结构
========

**为保证历史代码呈现的更为简洁，我们在该仓库中将 LATX 的近两千次提交合并为一次提交。**

下面是本项目的主要目录结构:

.. code-block:: text

   lat
   ├── ...
   ├── latxbuild/                           # 编译脚本
   ├── target/
   │   └── i386/
   │       └── latx/
   │           └── context/                 # 库直通相关
   │           └── convert.py               # 生成 LA 指令函数模板
   │           └── ir1/
   │               └── ir1.c                # IR1：x86 指令 IR 表示
   │           └── ir2/
   │               └── ir2.c                # IR2：LA 指令 IR 表示
   │               └── ir2-relocate.c       # label 处理等逻辑
   │               └── la-append.c          # 项目编译后由 convert.py 生成
   │               └── ir2-assemble.c
   │           └── latx-options.c           # LATX 功能选项设置
   │           └── optimization/
   │               └── flag-reduction.c     # TB 内 eflags 消除优化
   │               └── hbr.c                # 寄存器高位计算优化
   │               └── imm-cache.c          # 立即数加载优化
   │               └── insts-pattern.c      # 语义级指令组合优化翻译
   │               └── ir1-optimization.c   # IR1 层面优化扫描函数
   │               └── ir2-optimization.c   # IR2 层面指令调度函数
   │               └── tu.c                 # TU 翻译单元优化
   │               └── ...
   │           └── sbt/                     # AOT 相关
   │           └── translator/              # 翻译函数
   │               └── tr-logic.c           # 逻辑运算指令翻译函数
   │               └── tr-arith.c           # 算术运算指令翻译函数
   │               └── ...
   │           └── wrapper/                 # 库直通相关
   │           └── ...
   ├── ...
   └── README.rst                           # 本文档


主要贡献人员
============

有非常多的伙伴对本项目的成长做出了贡献，下面是代码提交量 Top10 的贡献人员。

1. **Lu Zeng <luzeng87@gmail.com>**

   - 项目 Owner，发起并领导着整体设计与架构
   - 推动性能优化的方案、评估与落地
   - 贡献众多兼容性与优化工作

2. **Hanlu Li <heuleehanlu@gmail.com>**

   - 项目 Maintainer
   - 16K 兼容方案影子页模块的作者
   - 贡献众多兼容性与优化工作

3. **Wenqiang Wei <weiwenqiang@mail.ustc.edu.cn>**

   - AOT 模块开发者之一，该模块的维护者
   - TU 优化开发者之一，该优化的维护者
   - 影子页模块维护者

4. **Jing Li <654224414@qq.com>**

   - AOT 早期开发者
   - 库直通模块维护者

5. **Qi Hu <spcreply@outlook.com>**

   - eflags 消除相关优化主要作者

6. **Yanzhi Lan <lanyanzhi19@mails.ucas.ac.cn>**

   - TU 优化开发者之一
   - 贡献很多指令级优化

7. **Chaoyi Liu <lcy285183897@gmail.com>**

   - insts-pattern 优化维护者
   - 指令级测试负责人

8. **Jinyang Shen <2509109915@qq.com>**

   - 早期开发人员
   - Capstone 模块部分优化工作

9. **Rengan Yue <y347812075@163.com>**

   - 软浮点模块及相关优化维护者

10. **Xiaotian Wu <yetist@gmail.com>**
    
    - 新世界适配

同时感谢在文档编写、社区管理、流程搭建、版本测试等方面做出贡献的所有伙伴。

编译
====

STEP1:


.. code-block:: bash

    git clone --depth=1 --recursive https://github.com/lat-opensource/lat
    cd lat


STEP2:

- debian

.. code-block:: bash

    apt install -y git ninja-build libssl-dev libc6 gcc g++ pkg-config libglib2.0-dev libdrm-dev lsb-release make python3-setuptools

- Arch Linux

.. code-block:: bash

    pacman -S --noconfirm ninja gcc pkgconf python3 python-setuptools openssl-static openssl

- 安同 OS (AOSC OS)

.. code-block:: bash

    oma install -y gcc nettle pcre2 libffi gnutls glib zlib glib-static libgcrypt-static libgpg-error-static libnfs-static pcre-static zlib-static zstd-static openssl-static pkg-config ninja


STEP3:

.. code-block:: bash

    ./latxbuild/build-release.sh

AVX指令支持
==============
编译支持AVX指令的版本需要在编译时添加参数
例如：

.. code-block:: bash

    ./latxbuild/build64.sh -c -a

为满足软件兼容性相关需求，LATX上报AVX指令相关CPUID信息需要单独设置：

.. code-block:: bash

    export LATX_AVX_CPUID=1


未来规划（TODO）
===============

项目未来的优化与完善方向包括但不限于：

- [ ] 支持更复杂的 x86 指令集扩展（如 AVX）。
- [ ] 进一步提升库直通优化的覆盖范围。
- [ ] 提供详细的性能分析工具链，帮助开发者快速定位性能瓶颈。
- [ ] 维护更详细的文档与使用指南。

欢迎大家通过 Issues 等方式讨论新特性需求、Bug 反馈以及优化思路。我们期待更
多开发者与社区力量的加入，一同推进 LoongArch  生态建设！

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

如有任何问题或建议，欢迎通过 Issue 与我们交流！

