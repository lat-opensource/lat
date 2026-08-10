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
    -Wl,--build-id=none "$source_file" -o "$workdir/prctl-x86-abi"

run_test()
{
    set +e
    LATX_AOT=0 LATX_KZT=0 timeout -s KILL 10 \
        "$emulator" "$workdir/prctl-x86-abi"
    ret=$?
    set -e

    case $ret in
0)
    echo "PASS: x86_64 prctl ABI behavior"
    ;;
20)
    echo "FAIL: PR_GET_PDEATHSIG pointer semantics" >&2
    ;;
21)
    echo "FAIL: PR_GET_AUXV guest copy semantics" >&2
    ;;
22)
    echo "FAIL: PR_GET_TSC/PR_SET_TSC semantics" >&2
    ;;
23)
    echo "FAIL: PR_GET_MDWE/PR_SET_MDWE enforcement" >&2
    ;;
24)
    echo "FAIL: RDTSC executed while PR_TSC_SIGSEGV was active" >&2
    ;;
25)
    echo "FAIL: RDTSC delivered an unexpected signal" >&2
    ;;
26)
    echo "FAIL: PR_SET_VMA pointer/address translation" >&2
    ;;
27)
    echo "FAIL: PR_SET_MM guest ABI translation" >&2
    ;;
28)
    echo "FAIL: x86 speculation-control emulation" >&2
    ;;
124)
    echo "FAIL: x86_64 prctl ABI test timed out" >&2
    ;;
*)
    echo "FAIL: unexpected guest exit status $ret" >&2
    ;;
    esac

    test "$ret" -eq 0
}

run_test
