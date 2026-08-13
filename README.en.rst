================================================
LATX (LoongArch Architecture Translator for x86)
================================================

.. image:: https://img.shields.io/badge/license-GPL--2.0-blue.svg
   :target: COPYING
   :alt: GPL-2.0 license

.. image:: https://github.com/lat-opensource/lat/actions/workflows/tests.yml/badge.svg
   :target: https://github.com/lat-opensource/lat/actions/workflows/tests.yml
   :alt: LAT Tests

LATX (LoongArch Architecture Translator for x86) is a high-performance
user-level binary translator designed specifically for the LoongArch
architecture, enabling efficient execution of x86 applications on
LoongArch-based systems.

Built upon QEMU 6 with substantial optimizations, LATX significantly
outperforms the original QEMU implementation.

The project leverages LoongArch's advanced instruction set extensions
(such as vector extensions and binary translation instructions) to
achieve high-efficiency translation of x86 instructions. Key optimizations
include AOT (Ahead-of-Time) compilation and runtime library pass-through,
with the latter partially inspired by and referencing source code from
the box64 project.

`中文版本点击这里 >> <README.rst>`_


Project Background
==================

As the LoongArch ecosystem evolves, compatibility and performance
bottlenecks arise when running legacy x86 applications. Existing
emulators such as vanilla QEMU cannot fully satisfy these demands
regarding efficiency and compatibility.

Therefore, we extended QEMU 6 with targeted optimizations, such as
AOT compilation and library pass-through, substantially reducing the
overhead of instruction translation and execution to achieve "faster,
more stable, and more compatible" performance.


Prerequisites
=============

- The build procedure in this README must run in a LoongArch Linux
  environment. The resulting translators also run on LoongArch Linux.
- LATX requires the host CPU and kernel to expose the LSX and LBT_X86
  extensions. Ordinary SIMD translations also use LASX when the host supports
  it. If LASX is not detected at startup, LATX disables its LASX paths and uses
  128-bit LSX instruction translations instead.
- LATX supports LoongArch ABI 1.0 and ABI 2.0. Configuration detects the ABI
  of the build environment, so build in an environment matching the target
  system's ABI.
- x86 Linux programs can run directly through LATX. Running x86 Windows
  programs also requires x86 Wine and a matching runtime environment.


Quick Start
===========

After installing the `Build Dependencies`_ on a LoongArch Linux host, run:

.. code-block:: bash

    git clone --depth=1 --recursive https://github.com/lat-opensource/lat
    cd lat
    ./latxbuild/build-release.sh

The script creates ``lat-<version>-<date>.tar.xz``. Install the newest package
and apply its binfmt and sysctl configuration immediately:

.. code-block:: bash

    package=$(ls -1t lat-*.tar.xz | head -n 1)
    sudo tar -Jxf "$package" -C / --strip-components=1
    sudo systemctl restart systemd-binfmt.service
    sudo systemctl restart systemd-sysctl.service

Rebooting also applies these settings. Check that both translators are
installed:

.. code-block:: bash

    latu-runtime-manager status

For the first test, use a statically linked x86_64 Linux program because it
does not need an additional x86 runtime:

.. code-block:: bash

    wget -O busybox.pkg.tar.zst \
        https://archlinux.org/packages/extra/x86_64/busybox/download/
    tar xf busybox.pkg.tar.zst
    latx-x86_64 ./usr/bin/busybox uname -m

After binfmt is registered, ``./usr/bin/busybox uname -m`` also works directly.
Dynamically linked programs require a matching x86 runtime under
``/usr/gnemul``; see the `build, installation, and runtime Wiki guide`_.


Build Dependencies
==================

Use the command for the host distribution. The lists also include ``file``,
``wget``, and ``zstd`` for the first BusyBox test. The build uses a system
Meson version of at least 0.55.3 when available. Otherwise, it uses the Meson
submodule fetched by ``--recursive``. Python 3.6 or newer is required.

Expand the entry for the host distribution:

.. raw:: html

   <details>
   <summary>Debian / Ubuntu</summary>

.. code-block:: bash

    sudo apt install -y git meson ninja-build libssl-dev libc6 gcc g++ \
        pkg-config libglib2.0-dev libdrm-dev lsb-release make python3 \
        python3-setuptools binutils file tar wget xz-utils zstd

.. raw:: html

   </details>
   <details>
   <summary>Arch Linux</summary>

.. code-block:: bash

    sudo pacman -S --needed git make meson ninja gcc pkgconf glib2 python \
        python-setuptools openssl binutils file tar wget xz zstd

.. raw:: html

   </details>
   <details>
   <summary>AOSC OS</summary>

.. code-block:: bash

    sudo oma install -y git make gcc meson nettle pcre2 libffi gnutls glib zlib \
        glib-static libgcrypt-static libgpg-error-static libnfs-static \
        pcre-static zlib-static zstd-static openssl-static pkg-config ninja \
        binutils file tar wget xz zstd

.. raw:: html

   </details>
   <details>
   <summary>Fedora</summary>

.. code-block:: bash

    sudo dnf install gcc gcc-c++ make git ninja-build meson openssl-devel \
        glib2-devel binutils file tar wget xz zstd

.. raw:: html

   </details>

Check the basic tools before building:

.. code-block:: bash

    python3 --version
    ninja --version


Build and Outputs
=================

The default release build creates both 32-bit and 64-bit translators:

.. code-block:: bash

    ./latxbuild/build-release.sh

The generated ``tar.xz`` package contains:

- ``usr/bin/latx-i386``: runs 32-bit i386 programs.
- ``usr/bin/latx-x86_64``: runs 64-bit x86_64 programs.
- ``usr/bin/latu-runtime-manager``: checks translator installation status.
- ``usr/lib/binfmt.d/*.conf``: registers binfmt rules for x86 ELF files.
- ``usr/lib/sysctl.d/mmap_min_addr.conf``: installs the mapping setting needed
  by LATX.

Packaging strips symbol tables and debug information, so packaged binaries are
smaller than the original outputs in ``build32/`` and ``build64/``. Use the
packaged binaries for normal installation and keep unstripped outputs for
debugging.


Running Dynamically Linked Programs
===================================

Dynamically linked x86 programs need a matching guest runtime. Display the
directory selected by each translator with:

.. code-block:: bash

    latx-x86_64 -runtime-info
    latx-i386 -runtime-info

Default paths depend on the host ABI. ABI 1.0 uses ``/usr/gnemul/latx-*`` and
ABI 2.0 uses ``/usr/gnemul/lat-*``. See the `build, installation, and runtime
Wiki guide`_ for runtime installation, Wine matching, upgrades, and removal.


Compatibility Scope and Known Limitations
=========================================

Guest program scope:

- Runs 32-bit (i386) and 64-bit (x86_64) x86 Linux user-space programs.
  Full-system x86 emulation is not provided.
- x86 Windows programs run through x86 Wine and require a matching runtime
  environment; see the `build, installation, and runtime Wiki guide`_.
- 16-bit real-mode (vm86) programs are outside the supported scope and are
  not verified.

x86 instruction set coverage:

- Release builds (O1 tier) translate and report x87, MMX, SSE, SSE2, and
  SSE3, plus SSSE3, SSE4.1, SSE4.2, POPCNT, AES, PCLMULQDQ, and CMPXCHG16B.
- AVX, AVX2, FMA, F16C, and BMI1/BMI2 belong to the testing optimization
  tier (-O 3). They are disabled by default and are not included in release
  packages. To use them, build manually as described in
  `AVX Instruction Support`_.

Runtime behavior notes:

- KZT wrappers (``LATX_KZT``) are available only in 64-bit builds. Run
  ``latx-x86_64 --kzt-libs`` to list the library groups, stability status, and
  dependencies provided by the installed version. 32-bit builds do not
  provide this option.
- Multi-threaded guests use the QEMU user-mode threading model; atomic
  instructions select an optimized path according to host kernel
  capabilities.
- Self-modifying code is invalidated per page by default. For programs with
  compatibility issues, adjust the ``LATX_SMC`` policy; for programs with
  abnormal floating-point results, try ``LATX_SOFTFPU`` (which reduces
  performance and automatically disables AOT). The
  `LATX configuration reference <docs/user/latx-environment.rst>`_ describes
  these options and is currently available in Chinese.


Configuration
=============

LATX supports system and user configuration files, environment variables, and
command-line options. The `LATX configuration reference
<docs/user/latx-environment.rst>`_ describes precedence and common settings.
This reference is currently available in Chinese.

Common KZT library-group controls:

.. code-block:: bash

    # List the groups provided by this version
    latx-x86_64 --kzt-libs

    # Enable the current stable set
    LATX_KZT=1 latx-x86_64 /path/to/program

    # Remove one group and its dependents from the stable set
    LATX_KZT=1 LATX_KZT_LIBS=-x11 latx-x86_64 /path/to/program

    # Select one group and its required dependencies
    LATX_KZT=1 LATX_KZT_LIBS=x11 latx-x86_64 /path/to/program


AVX Instruction Support
=======================

``build-release.sh`` does not currently provide an AVX packaging option. Both
``build32.sh`` and ``build64.sh`` accept ``-a``. The option passes
``--enable-latx-avx-opt`` to ``configure``, enabling x86 AVX instruction
translation support for the selected build target:

.. code-block:: bash

    ./latxbuild/build32.sh -c -a
    ./latxbuild/build64.sh -c -a

The ``-c`` option regenerates the build configuration and must be included
when changing AVX support.

An AVX build reports the corresponding CPUID information to the guest by
default. To hide it, set the following before starting the guest:

.. code-block:: bash

    export LATX_AVX_CPUID=0

This setting changes CPUID reporting. It does not disable the compiled AVX
instruction translators and cannot be changed while the guest is running.


Documentation and Support
=========================

- `LAT Wiki <https://github.com/lat-opensource/lat/wiki>`_
- `Build, installation, and runtime Wiki guide`_
- `Troubleshooting guide`_
- `Issues <https://github.com/lat-opensource/lat/issues>`_
- `Discussions <https://github.com/lat-opensource/lat/discussions>`_

The Wiki guides are currently available in Chinese.

Before opening a pull request, read the `contribution guide
<CONTRIBUTING.md>`_ and `commit convention <COMMIT_CONVENTION.en.md>`_.
Every commit must include a DCO ``Signed-off-by`` trailer.


Contributors
============

.. raw:: html

   <table>
     <tr>
       <td align="center" colspan="8"><b>Internal development phase (2021–2024)</b></td>
     </tr>
     <tr>
       <td align="center"><a href="https://github.com/luzeng87"><img src="https://github.com/luzeng87.png?size=64" width="64" height="64" alt="Lu Zeng"><br><sub><b>Lu Zeng</b></sub></a></td>
       <td align="center"><a href="https://github.com/LaurenIsACoder"><img src="https://github.com/LaurenIsACoder.png?size=64" width="64" height="64" alt="Hanlu Li"><br><sub><b>Hanlu Li</b></sub></a></td>
       <td align="center"><a href="https://github.com/ganjue66da"><img src="https://github.com/ganjue66da.png?size=64" width="64" height="64" alt="Wenqiang Wei"><br><sub><b>Wenqiang Wei</b></sub></a></td>
       <td align="center"><a href="https://github.com/JonLeeTaoShan"><img src="https://github.com/JonLeeTaoShan.png?size=64" width="64" height="64" alt="Jing Li"><br><sub><b>Jing Li</b></sub></a></td>
       <td align="center"><a href="https://github.com/specialpointcentral"><img src="https://github.com/specialpointcentral.png?size=64" width="64" height="64" alt="Qi Hu"><br><sub><b>Qi Hu</b></sub></a></td>
       <td align="center"><a href="https://github.com/rmjskhy"><img src="https://github.com/rmjskhy.png?size=64" width="64" height="64" alt="Chaoyi Liu"><br><sub><b>Chaoyi Liu</b></sub></a></td>
       <td align="center"><a href="https://github.com/y347812075"><img src="https://github.com/y347812075.png?size=64" width="64" height="64" alt="Rengan Yue"><br><sub><b>Rengan Yue</b></sub></a></td>
       <td align="center"><a href="https://github.com/yetist"><img src="https://github.com/yetist.png?size=64" width="64" height="64" alt="Xiaotian Wu"><br><sub><b>Xiaotian Wu</b></sub></a></td>
     </tr>
     <tr>
       <td align="center" colspan="8"><b>Open-source community (2025–)</b></td>
     </tr>
     <tr>
       <td align="center"><a href="https://github.com/NiuGenen"><img src="https://github.com/NiuGenen.png?size=64" width="64" height="64" alt="NiuGenen"><br><sub><b>NiuGenen</b></sub></a></td>
       <td align="center"><a href="https://github.com/xiezyang"><img src="https://github.com/xiezyang.png?size=64" width="64" height="64" alt="Zhaoyang Xie"><br><sub><b>Zhaoyang Xie</b></sub></a></td>
       <td align="center"><a href="https://github.com/xiangzhai"><img src="https://github.com/xiangzhai.png?size=64" width="64" height="64" alt="Leslie Zhai"><br><sub><b>Leslie Zhai</b></sub></a></td>
       <td align="center"><a href="https://github.com/sunhaiyong1978"><img src="https://github.com/sunhaiyong1978.png?size=64" width="64" height="64" alt="Sun Haiyong"><br><sub><b>Sun Haiyong</b></sub></a></td>
       <td align="center"><a href="https://github.com/baibaidashixiong"><img src="https://github.com/baibaidashixiong.png?size=64" width="64" height="64" alt="zqz"><br><sub><b>zqz</b></sub></a></td>
       <td align="center"><a href="https://github.com/zhaodongru"><img src="https://github.com/zhaodongru.png?size=64" width="64" height="64" alt="Dongru Zhao"><br><sub><b>Dongru Zhao</b></sub></a></td>
       <td align="center"><a href="https://github.com/phorcys"><img src="https://github.com/phorcys.png?size=64" width="64" height="64" alt="phorcys"><br><sub><b>phorcys</b></sub></a></td>
       <td align="center"><a href="https://github.com/wojiushixiaobai"><img src="https://github.com/wojiushixiaobai.png?size=64" width="64" height="64" alt="wojiushixiaobai"><br><sub><b>wojiushixiaobai</b></sub></a></td>
     </tr>
   </table>

The avatars above show a subset of contributors; the order is for
presentation only and implies no contribution ranking.

LATX thanks every contributor. See `CONTRIBUTORS.md <CONTRIBUTORS.md>`_
for contributors of each development phase and their contribution areas,
and `GitHub Contributors
<https://github.com/lat-opensource/lat/graphs/contributors>`_ for the
post-open-source record.


License
========

This project is a secondary development based on the QEMU. The original QEMU project
is released under the GNU General Public License version 2 (GPLv2).

Accordingly, this project is also licensed under the terms of the GPLv2.


Acknowledgments
===============

Special thanks to the QEMU and box64 projects and their developers for
their invaluable open-source contributions and support.

------------

If you have any questions or suggestions, please feel free to engage with us through `Issue <https://github.com/lat-opensource/lat/issues>`_ !


.. _build, installation, and runtime Wiki guide: https://github.com/lat-opensource/lat/wiki/%E7%BC%96%E8%AF%91%E4%B8%8E%E8%BF%90%E8%A1%8C
.. _Troubleshooting guide: https://github.com/lat-opensource/lat/wiki/%E8%B0%83%E8%AF%95%E4%B8%8E%E9%97%AE%E9%A2%98%E5%AE%9A%E4%BD%8D%E6%8C%87%E5%8D%97
