#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-or-later
set -eu
emulator=$1
source_file=$2
case $(uname -m) in
    loongarch64) ;;
    *) echo "SKIP: LoongArch host required"; exit 77 ;;
esac
workdir=$(mktemp -d)
trap 'rm -rf "$workdir"' EXIT HUP INT TERM
if [ "$(getconf PAGESIZE)" != 16384 ]; then
    echo "SKIP: shadow fragment regression requires 16K host pages"
    exit 77
fi
# Match the production fast-atomic gate, including byte/halfword support.
if ! command -v cc >/dev/null 2>&1; then
    echo "SKIP: native C compiler required for CPU capability probe"
    exit 77
fi
cat > "$workdir/probe.c" <<'C'
int main(void)
{
    unsigned int cfg, index = 2;
    unsigned int mask = (1U << 22) | (1U << 27) | (1U << 28);
    __asm__ volatile("cpucfg %0,%1" : "=r"(cfg) : "r"(index));
    return (cfg & mask) == mask ? 0 : 77;
}
C
cc -O2 "$workdir/probe.c" -o "$workdir/probe"
"$workdir/probe"

fixture=${LATX_SHADOW_ATOMIC_FIXTURE:-}
if [ -n "$fixture" ]; then
    test -x "$fixture"
    # A stale prebuilt fixture must not silently validate different test code.
    test "$(sha256sum "$source_file" | cut -d' ' -f1)" = \
         "$(cat "$fixture.source-sha256")"
    test "$(sha256sum "$fixture" | cut -d' ' -f1)" = \
         "$(cat "$fixture.sha256")"
    guest=$fixture
else
    cc=${LATX_X86_64_CC:-clang}
    if ! command -v "$cc" >/dev/null 2>&1 ||
       ! command -v ld.lld >/dev/null 2>&1; then
        echo "SKIP: clang/LLD or LATX_SHADOW_ATOMIC_FIXTURE required"
        exit 77
    fi
    # Probe the toolchain independently: errors in the real fixture must fail.
    cat > "$workdir/cross-probe.c" <<'C'
void _start(void)
{
    __asm__ volatile("syscall" : : "a"(60), "D"(0) : "rcx", "r11", "memory");
    __builtin_unreachable();
}
C
    if ! "$cc" --target=x86_64-linux-gnu -fuse-ld=lld -nostdlib \
        -static -no-pie -O2 -ffreestanding -fno-builtin \
        -fno-stack-protector "$workdir/cross-probe.c" \
        -o "$workdir/cross-probe"; then
        echo "SKIP: compiler cannot link a freestanding x86_64 program"
        exit 77
    fi
    guest=$workdir/guest
    "$cc" --target=x86_64-linux-gnu -fuse-ld=lld -nostdlib \
        -static -no-pie -O2 -ffreestanding -fno-builtin \
        -fno-stack-protector "$source_file" -o "$guest"
fi
LATX_AOT=0 LATX_KZT=0 LATX_CLOSE_PARALLEL=1 timeout -s KILL 30 \
    "$emulator" "$guest" "$workdir/data"
echo 'PASS: shadow B/H atomics, flags, permissions and process contention'
