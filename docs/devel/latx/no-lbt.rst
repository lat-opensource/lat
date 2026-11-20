Running without the LoongArch LBT extension
==========================================

LAT still requires LSX. LBT and LASX are detected independently at startup.
The host ``/proc/cpuinfo`` Features/flags records are intersected across CPUs
and checked against Linux ``AT_HWCAP``. A feature absent from either source
is disabled. If cpuinfo cannot be read or has no feature records, LAT uses
AT_HWCAP alone. Missing LSX stops startup with an explicit error.

``LATX_LBT=1`` (default), or ``-latx-lbt 1``, selects automatic detection.
``LATX_LBT=0`` or ``-latx-lbt 0`` selects the software backend even on an
LBT-capable host. These values also work in LAT configuration files using
the normal option precedence. The value 1 never enables unsupported hardware.
Invalid values are rejected.

The software backend stores arithmetic flags in the mapped EFLAGS register
and x87 state in CPU memory. It defaults to ``LATX_SOFTFPU=2`` when softfpu
would otherwise be off. An explicit softfpu mode of 1 is retained. LBT-based
JR/RA handling, native tunnel libraries, VPAES, fast atomics, KZT, hardware
x87 tags and instruction fusion are disabled after all options are parsed.
AOT and TU remain available. This port contains no application addresses,
application-specific native kernels or hotspot TB replacements.

Implementation locations
------------------------

* ``host-features.c`` parses cpuinfo; ``latx-options.c`` chooses the final mode.
* ``tr-state.c`` implements EFLAGS and x87 TOP access. Callers use explicit
  functions declared in ``flag-state.h``; raw emitters are not macro-redefined.
* ``tr-eflag-soft.c`` computes software arithmetic flags, using the shared
  per-flag fallbacks declared in ``translator/eflag-internal.h``.
* ``tr-fcvt.c`` saves live host state around software float80 conversions.
* ``tr-memory.c`` provides exact-width scalar/byte fallbacks for unaligned
  vector and double accesses. The guest page checks remain at their callers.

Generated AOT filenames include LBT, LASX, softfpu and softfpu-fast modes.
Switching modes therefore cannot load a cache made for a different register
or floating-point backend. Cache versioning still uses upstream's source
and build configuration identity.

Some older LSX hosts also need scalar host libc routines because their
libc selects vector routines that assume unaligned access support. Such
static builds can run ``python3 meson/meson.py configure build64
-Dlatx_host_scalar_libc=true`` after configuration, then rebuild, with a
glibc providing the ``__*_aligned`` implementations. This is a
build-time compatibility option, not a replacement for guest translation.

Validation
----------

Configure with ``--enable-tests``. Run the complete ``lat-pr-fast`` suite
and the no-LBT integration tests, including ``test-lbt-options`` and
``test-eflags-differential-no-lbt``. The option test verifies AOT loading
with ``LATX_TEST_AOT_LOAD_MARKER``; file creation alone is insufficient.
Dynamic builds additionally test automatic detection with a test-only
cpuinfo override. Static builds report that subcase as unavailable.

When no x86 compiler is available on LoongArch, the new runners accept
``LATX_NO_LBT_GUEST_DIR`` containing guests built from the same checkout.
Compile the assembly with ``gcc -nostdlib -static -no-pie`` on x86 Linux.
Use each source basename as the executable name, with these exceptions:

* ``lua-number-conversion-no-lbt.S`` also supplies ``lbt-options``.
* ``latx-eflags-branch-differential.S`` supplies ``eflags-branch``.
* ``latx-eflags-differential.S`` supplies ``eflags-random``; compile with
  ``-DTRACE_RECORD=1``.

Run the two EFLAGS guests natively and save their stdout as
``branch.reference`` and ``random.reference`` in that directory. The
comparator masks architecturally undefined flags. Without prebuilt guests,
the differential runner uses ``qemu-x86_64`` as the reference.

Forced software mode and injected cpuinfo on an LBT host test translation
and option handling. They do not replace testing on physical hardware
without LBT, including its kernel, libc and alignment behavior.
