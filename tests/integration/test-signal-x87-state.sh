#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-only
set -eu

emulator=$1
source_file=$2
arch=${3:-x86_64}
signal_flags=-UTEST_LEGACY_SIGNAL
case "$arch" in
    x86_64) bits=-m64 ;;
    i386) bits=-m32 ;;
    i386-legacy)
        arch=i386
        bits=-m32
        signal_flags=-DTEST_LEGACY_SIGNAL
        ;;
    *) echo "unsupported guest architecture: $arch" >&2; exit 1 ;;
esac
workdir=$(mktemp -d)
trap 'rm -rf "$workdir"' EXIT HUP INT TERM

compiler=
if command -v clang-19 >/dev/null 2>&1 &&
        command -v ld.lld >/dev/null 2>&1; then
    clang=clang-19
    compiler=clang
elif command -v clang >/dev/null 2>&1 &&
        command -v ld.lld >/dev/null 2>&1; then
    clang=clang
    compiler=clang
elif [ "$(uname -m)" = x86_64 ] && command -v "${CC:-cc}" >/dev/null 2>&1; then
    compiler=native
else
    echo "SKIP: an x86_64 compiler or clang with ld.lld is required"
    exit 77
fi

compile_case()
{
    name=$1
    number=$2

    if [ "$compiler" = clang ]; then
        "$clang" --target="$arch-linux-gnu" -fuse-ld=lld -nostdlib \
            -static -no-pie -O2 -ffreestanding -fno-builtin \
            -fno-stack-protector -mno-sse -mno-sse2 -mmmx \
            -Wl,--build-id=none "$signal_flags" \
            -DTEST_CASE="$number" "$source_file" \
            -o "$workdir/$name"
    else
        "${CC:-cc}" "$bits" -nostdlib -static -no-pie -O2 -ffreestanding \
            -fno-builtin -fno-stack-protector -mno-sse -mno-sse2 -mmmx \
            -Wl,--build-id=none "$signal_flags" \
            -DTEST_CASE="$number" "$source_file" \
            -o "$workdir/$name"
    fi
}

run_case()
{
    name=$1
    mode=$2

    set +e
    timeout 10s env LATX_AOT=0 LATX_KZT=0 LATX_SOFTFPU="$mode" \
        "$emulator" "$workdir/$name"
    ret=$?
    set -e

    if [ "$ret" -eq 0 ]; then
        echo "PASS: $name with LATX_SOFTFPU=$mode"
        return
    fi
    if [ "$ret" -eq 77 ] && [ "$name" = avx-entry ]; then
        echo "SKIP: guest AVX/xsave is unavailable"
        return
    fi

    case "$ret" in
    10) reason="rt_sigaction failed" ;;
    11) reason="SIGUSR1 delivery failed" ;;
    21) reason="handler inherited interrupted x87 state" ;;
    22) reason="sigreturn corrupted the x87 stack" ;;
    31) reason="sigreturn corrupted the x87 control word" ;;
    32) reason="sigreturn left the wrong host rounding mode" ;;
    41) reason="sigreturn left the wrong MMX register state" ;;
    51) reason="handler inherited interrupted MXCSR" ;;
    52) reason="sigreturn corrupted MXCSR" ;;
    53) reason="handler inherited interrupted vector state" ;;
    54) reason="sigreturn corrupted vector state" ;;
    61) reason="sigreturn misclassified nonzero x87 TOP as MMX" ;;
    71) reason="sigreturn lost the x87 precision flag" ;;
    72) reason="signal frame lost the x87 precision flag" ;;
    81) reason="signal round-trip lost the x87 denormal flag" ;;
    91) reason="handler exception flags leaked past sigreturn" ;;
    101) reason="SSE exception flag leaked into the x87 signal state" ;;
    102) reason="signal frame lost the SSE exception flag" ;;
    103) reason="handler inherited interrupted FP exception state" ;;
    104) reason="sigreturn copied the SSE flag into x87 state" ;;
    105) reason="sigreturn lost the SSE exception flag" ;;
    111) reason="signal frame mixed x87 and SSE exception flags" ;;
    112) reason="mixed exception flags leaked into the handler" ;;
    113) reason="sigreturn mixed x87 and SSE exception flags" ;;
    124) reason="test timed out" ;;
    *) reason="unexpected exit status $ret" ;;
    esac
    echo "FAIL: $name with LATX_SOFTFPU=$mode: $reason" >&2
    exit "$ret"
}

compile_case x87-entry 1
compile_case x87-rounding 2
compile_case mmx-restore 3
compile_case sse-entry 4
compile_case avx-entry 5
if [ "$signal_flags" != -DTEST_LEGACY_SIGNAL ]; then
    compile_case x87-nonzero-top 6
    compile_case x87-exception-flags 7
    compile_case x87-denormal-flag 8
    compile_case handler-flag-leak 9
    compile_case sse-exception-flags 10
    compile_case mixed-exception-flags 11
fi

for mode in 0 1 2; do
    run_case x87-entry "$mode"
    run_case x87-rounding "$mode"
    run_case mmx-restore "$mode"
    run_case sse-entry "$mode"
    run_case avx-entry "$mode"
    if [ "$signal_flags" != -DTEST_LEGACY_SIGNAL ]; then
        run_case x87-nonzero-top "$mode"
        run_case x87-exception-flags "$mode"
        run_case x87-denormal-flag "$mode"
        run_case handler-flag-leak "$mode"
        run_case sse-exception-flags "$mode"
        run_case mixed-exception-flags "$mode"
    fi
done
