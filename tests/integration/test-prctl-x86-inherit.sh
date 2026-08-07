#!/bin/sh
set -eu

emulator=$1
source_file=$2
workdir=$(mktemp -d)
trap 'rm -rf "$workdir"' EXIT HUP INT TERM

if command -v clang-19 >/dev/null 2>&1; then
    clang=clang-19
elif command -v clang >/dev/null 2>&1; then
    clang=clang
else
    echo "SKIP: clang is required to build the x86_64 guest"
    exit 77
fi

"$clang" --target=x86_64-linux-gnu -fuse-ld=lld -nostdlib -static \
    -Wl,--build-id=none "$source_file" -o "$workdir/prctl-x86-inherit"

run_mode()
{
    mode=$1
    label=$2

    set +e
    LATX_AOT=0 LATX_KZT=0 LATX_TU="$mode" timeout -s KILL 10 \
        "$emulator" "$workdir/prctl-x86-inherit" "$emulator"
    ret=$?
    set -e

    case $ret in
0)
    echo "PASS: $label x86_64 prctl state inheritance"
    ;;
31)
    echo "FAIL: unexpected exec test stage" >&2
    ;;
32)
    echo "FAIL: guest self-exec failed" >&2
    ;;
33)
    echo "FAIL: inherited PR_TSC_SIGSEGV did not trap RDTSC" >&2
    ;;
34)
    echo "FAIL: PR_MDWE inheritance semantics" >&2
    ;;
35)
    echo "FAIL: PR_TSC inheritance semantics" >&2
    ;;
36)
    echo "FAIL: inherited TSC mode delivered an unexpected signal" >&2
    ;;
124)
    echo "FAIL: x86_64 prctl inheritance test timed out" >&2
    ;;
*)
    echo "FAIL: $label unexpected guest exit status $ret" >&2
    ;;
    esac

    test "$ret" -eq 0
}

run_mode 0 non-tu
run_mode 1 tu
