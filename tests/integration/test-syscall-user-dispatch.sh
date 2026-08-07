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
    -Wl,--build-id=none "$source_file" -o "$workdir/syscall-user-dispatch"

run_mode()
{
    mode=$1
    label=$2

    set +e
    LATX_AOT=0 LATX_KZT=0 LATX_TU="$mode" timeout -s KILL 10 \
        "$emulator" "$workdir/syscall-user-dispatch"
    ret=$?
    set -e

    case $ret in
0)
    echo "PASS: $label syscall user dispatch delivered guest SIGSYS"
    ;;
10)
    echo "FAIL: blocked guest syscall was executed" >&2
    ;;
11)
    echo "FAIL: syscall user dispatch setup failed" >&2
    ;;
12)
    echo "FAIL: guest SIGSYS metadata was invalid" >&2
    ;;
13)
    echo "FAIL: unknown prctl was not rejected with EINVAL" >&2
    ;;
14)
    echo "FAIL: syscall user dispatch was inherited across fork" >&2
    ;;
15)
    echo "FAIL: syscall user dispatch parameter validation" >&2
    ;;
16)
    echo "FAIL: syscall inside the allowed dispatch range was blocked" >&2
    ;;
17)
    echo "FAIL: PR_SYS_DISPATCH_OFF did not disable dispatch" >&2
    ;;
124)
    echo "FAIL: syscall user dispatch test timed out" >&2
    ;;
159)
    echo "FAIL: guest syscall dispatch leaked into the host (SIGSYS)" >&2
    ;;
*)
    echo "FAIL: $label unexpected guest exit status $ret" >&2
    ;;
    esac

    test "$ret" -eq 0
}

run_mode 0 non-tu
run_mode 1 tu
