#!/bin/sh
set -eu

emulator=$1
source_file=$2
workdir=$(mktemp -d)
trap 'rm -rf "$workdir"' EXIT HUP INT TERM
aot_home=$workdir/aot-home
mkdir -p "$aot_home"

if command -v clang-19 >/dev/null 2>&1; then
    clang=clang-19
elif command -v clang >/dev/null 2>&1; then
    clang=clang
else
    echo "SKIP: clang is required to build the i386 guest"
    exit 77
fi

"$clang" --target=i386-linux-gnu -fuse-ld=lld -nostdlib -static \
    -Wl,--build-id=none "$source_file" \
    -o "$workdir/futex-transient-wait-race-i386"

for aot in 0 1; do
    for kzt in 0 1; do
        set +e
        HOME="$aot_home" LATX_AOT="$aot" LATX_KZT="$kzt" timeout -s KILL 20 \
            "$emulator" "$workdir/futex-transient-wait-race-i386"
        ret=$?
        set -e

        case $ret in
        0) echo "PASS: AOT=$aot KZT=$kzt i386 private futex waits survived concurrent unaligned and overlapping locked operations" ;;
        14) echo "FAIL: AOT=$aot KZT=$kzt i386 private futex wait returned EFAULT" >&2 ;;
        2) echo "FAIL: AOT=$aot KZT=$kzt primary locked counter lost updates" >&2 ;;
        3) echo "FAIL: AOT=$aot KZT=$kzt overlapping CMPXCHG8B low counter lost updates" >&2 ;;
        4) echo "FAIL: AOT=$aot KZT=$kzt overlapping CMPXCHG8B high counter lost updates" >&2 ;;
        124) echo "FAIL: AOT=$aot KZT=$kzt i386 futex race test timed out" >&2 ;;
        *) echo "FAIL: AOT=$aot KZT=$kzt unexpected i386 futex race status $ret" >&2 ;;
        esac

        test "$ret" -eq 0
    done
done
