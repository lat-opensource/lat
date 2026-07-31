#!/bin/sh
set -eu

emulator=$1
source_file=$2
workdir=$(mktemp -d)
trap 'rm -rf "$workdir"' EXIT HUP INT TERM

runtime_prefix="$workdir/runtime"
mkdir -p "$runtime_prefix/proc"

if command -v clang-19 >/dev/null 2>&1; then
    clang=clang-19
elif command -v clang >/dev/null 2>&1; then
    clang=clang
else
    echo "SKIP: clang is required to build the x86_64 guest"
    exit 77
fi

"$clang" --target=x86_64-linux-gnu -fuse-ld=lld -nostdlib -static -no-pie \
    -Wl,--build-id=none "$source_file" -o "$workdir/proc-readdir"

unset LATX_AOT LATX_TU LATX_KZT

run_case()
{
    label=$1
    shift

    set +e
    "$@" "$emulator" -L "$runtime_prefix" "$workdir/proc-readdir"
    ret=$?
    set -e

    case $ret in
    0)
        echo "PASS: $label enumerated a numeric /proc entry"
        ;;
    10)
        echo "FAIL: $label could not open /proc" >&2
        ;;
    11)
        echo "FAIL: $label getdents64 failed" >&2
        ;;
    12)
        echo "FAIL: $label found no numeric /proc entry" >&2
        ;;
    13)
        echo "FAIL: $label received a malformed dirent" >&2
        ;;
    *)
        echo "FAIL: $label returned unexpected status $ret" >&2
        ;;
    esac

    if [ "$ret" -ne 0 ]; then
        exit "$ret"
    fi
}

run_case default env
run_case non-tu env LATX_AOT=0 LATX_TU=0
run_case tu env LATX_AOT=0 LATX_TU=1
