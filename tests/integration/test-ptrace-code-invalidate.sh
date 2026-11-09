#!/bin/sh
set -eu
emulator=$1
source_file=$2
workdir=$(mktemp -d)
trap 'rm -rf "$workdir"' EXIT HUP INT TERM
set --
if [ -n "${X86_64_CC:-}" ]; then
    compiler=$X86_64_CC
elif command -v x86_64-linux-gnu-gcc >/dev/null 2>&1; then
    compiler=x86_64-linux-gnu-gcc
elif [ "$(uname -m)" = x86_64 ] && command -v gcc >/dev/null 2>&1; then
    compiler=gcc
elif command -v clang-19 >/dev/null 2>&1; then
    compiler=clang-19
    set -- --target=x86_64-linux-gnu -fuse-ld=lld
elif command -v clang >/dev/null 2>&1; then
    compiler=clang
    set -- --target=x86_64-linux-gnu -fuse-ld=lld
else
    echo "SKIP: x86_64 GCC or clang/lld is required (or set X86_64_CC)"
    exit 77
fi
"$compiler" "$@" -nostdlib -static -no-pie \
    -O2 -ffreestanding -fno-builtin -fno-stack-protector \
    -Wl,--build-id=none "$source_file" -o "$workdir/guest"
set +e
env LATX_AOT=0 LATX_KZT=0 LATX_FAST_TRANSLATOR=1 \
    LATX_FAST_TRANSLATOR_STRICT=1 timeout -k 2 20 "$emulator" "$workdir/guest"
ret=$?
set -e
case $ret in
0) echo "PASS: single/multiple-page ptrace writes, CONT and DETACH" ;;
77) echo "SKIP: kernel denied PTRACE_TRACEME" ;;
*) echo "FAIL: ptrace code invalidation, guest status $ret" >&2 ;;
esac
exit "$ret"
