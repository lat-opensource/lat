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

Debian / Ubuntu:

.. code-block:: bash

    sudo apt install -y git meson ninja-build libssl-dev libc6 gcc g++ \
        pkg-config libglib2.0-dev libdrm-dev lsb-release make python3 \
        python3-setuptools binutils file tar wget xz-utils zstd

Arch Linux:

.. code-block:: bash

    sudo pacman -S --needed git make meson ninja gcc pkgconf glib2 python \
        python-setuptools openssl binutils file tar wget xz zstd

AOSC OS:

.. code-block:: bash

    sudo oma install -y git make gcc meson nettle pcre2 libffi gnutls glib zlib \
        glib-static libgcrypt-static libgpg-error-static libnfs-static \
        pcre-static zlib-static zstd-static openssl-static pkg-config ninja \
        binutils file tar wget xz zstd

Fedora:

.. code-block:: bash

    sudo dnf install gcc gcc-c++ make git ninja-build meson openssl-devel \
        glib2-devel binutils file tar wget xz zstd

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


Configuration
=============

LATX supports system and user configuration files, environment variables, and
command-line options. The `LATX configuration reference
<docs/user/latx-environment.rst>`_ describes precedence and common settings.
This reference is currently available in Chinese.


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
<CONTRIBUTING.md>`_ and `commit convention <COMMIT_CONVENTION.en.md>`_. See
`CONTRIBUTORS.md <CONTRIBUTORS.md>`_ for the historical major-contributor list.
Every commit must include a DCO ``Signed-off-by`` trailer.


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
