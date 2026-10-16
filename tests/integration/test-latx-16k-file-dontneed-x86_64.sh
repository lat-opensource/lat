#!/bin/sh
set -eu

emulator=$1
source_file=$2
workdir=$(mktemp -d)
trap 'rm -rf "$workdir"' EXIT HUP INT TERM

if [ "$(getconf PAGESIZE)" != 16384 ]; then
    echo "SKIP: the emulated file mapping requires a 16K host page"
    exit 77
fi

fixture=${LATX_16K_FILE_DONTNEED_FIXTURE:-}
if [ -n "$fixture" ]; then
    if [ ! -f "$fixture" ] || [ ! -x "$fixture" ]; then
        echo "FAIL: prebuilt x86_64 fixture is not executable: $fixture" >&2
        exit 1
    fi
    guest=$fixture
else
    if command -v clang-19 >/dev/null 2>&1; then
        clang=clang-19
    elif command -v clang >/dev/null 2>&1; then
        clang=clang
    else
        echo "SKIP: clang is required; alternatively set LATX_16K_FILE_DONTNEED_FIXTURE"
        exit 77
    fi
    if ! command -v ld.lld >/dev/null 2>&1; then
        echo "SKIP: ld.lld is required; alternatively set LATX_16K_FILE_DONTNEED_FIXTURE"
        exit 77
    fi
    guest=$workdir/latx-16k-file-dontneed
    "$clang" --target=x86_64-linux-gnu -fuse-ld=lld -nostdlib -static -no-pie \
        -O2 -ffreestanding -fno-builtin -fno-stack-protector \
        -Wl,--build-id=none "$source_file" -o "$guest"
fi

set +e
LATX_AOT=0 LATX_KZT=0 timeout -s KILL 30 \
    "$emulator" "$guest" "$workdir/data"
ret=$?
set -e

case $ret in
0) echo "PASS: MADV_DONTNEED preserves an emulated file mapping" ;;
9) echo "FAIL: guest argument setup failed" >&2; exit 1 ;;
10) echo "FAIL: guest open failed" >&2; exit 1 ;;
11) echo "FAIL: guest ftruncate failed" >&2; exit 1 ;;
12) echo "FAIL: guest write failed" >&2; exit 1 ;;
13) echo "FAIL: guest read failed" >&2; exit 1 ;;
14) echo "FAIL: guest reservation mmap failed" >&2; exit 1 ;;
15) echo "FAIL: guest fixed file mmap failed" >&2; exit 1 ;;
16) echo "FAIL: guest madvise failed" >&2; exit 1 ;;
17) echo "FAIL: guest lseek failed" >&2; exit 1 ;;
20) echo "FAIL: file contents wrong before MADV_DONTNEED" >&2; exit 1 ;;
21) echo "FAIL: MADV_DONTNEED discarded the emulated file contents" >&2; exit 1 ;;
124|137) echo "FAIL: emulated file mapping timed out" >&2; exit 1 ;;
*) echo "FAIL: unexpected status $ret" >&2; exit 1 ;;
esac
