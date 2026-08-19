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
    -Wl,--build-id=none "$source_file" -o "$workdir/int3-sigtrap-rip"

for mode in 0 2; do
    set +e
    timeout 10s env LATX_AOT=0 LATX_KZT=$mode LATX_KZT_LIBS=core \
        "$emulator" "$workdir/int3-sigtrap-rip"
    ret=$?
    set -e

    case $ret in
    0) ;;
    10) echo "FAIL: rt_sigaction failed with LATX_KZT=$mode" >&2; exit $ret ;;
    11) echo "FAIL: wrong signal reached the handler with LATX_KZT=$mode" >&2; exit $ret ;;
    12) echo "FAIL: terminal INT3 restored the wrong RIP with LATX_KZT=$mode" >&2; exit $ret ;;
    13) echo "FAIL: INT3 did not deliver SIGTRAP with LATX_KZT=$mode" >&2; exit $ret ;;
    124) echo "FAIL: terminal INT3 test timed out with LATX_KZT=$mode" >&2; exit $ret ;;
    *) echo "FAIL: unexpected exit status $ret with LATX_KZT=$mode" >&2; exit $ret ;;
    esac
done

echo "PASS: terminal INT3 preserves SIGTRAP next RIP"
