#!/bin/sh
set -eu
emulator=$(readlink -f "$1")
source_file=$(readlink -f "$2")
shim_source=$(readlink -f "$3")
workdir=$(mktemp -d)
trap 'rm -rf "$workdir"' EXIT HUP INT TERM
. "$(dirname "$(readlink -f "$0")")/no-lbt-test-lib.sh"
no_lbt_build_guest "$source_file" "$workdir/lbt-options"

# Force cache generation; test both argument orders for softfpu and AOT.
for order in a b; do
    cache_home="$workdir/$order"
    mkdir -p "$cache_home"
    case "$order" in
        a) set -- -latx-aot 1 -latx-softfpu 2 ;;
        b) set -- -latx-softfpu 2 -latx-aot 1 ;;
    esac
    HOME="$cache_home" LATX_AOT_GENERATE=1 LATX_LBT=1 \
        "$emulator" -latx-lbt 0 "$@" "$workdir/lbt-options"
    count=0
    while ! find "$cache_home" -name '*-lbt0-lasx*-fpu2-fast*.aot2' -size +0c | grep -q .; do
        count=$((count + 1))
        test "$count" -lt 10
        sleep 1
    done
    marker="$cache_home/loaded"
    HOME="$cache_home" LATX_AOT_GENERATE=0 LATX_TEST_AOT_LOAD_MARKER="$marker" \
        "$emulator" -latx-lbt 0 "$@" "$workdir/lbt-options"
    test -s "$marker"
done

if "$emulator" -latx-lbt 2 "$workdir/lbt-options" >"$workdir/invalid.log" 2>&1; then
    echo 'FAIL: invalid LBT value accepted'
    exit 1
fi
grep -q 'LATX_LBT must be' "$workdir/invalid.log"

# Dynamic builds also exercise automatic detection and option finalization
# after config/env/arguments. A missing LSX record proves the override is used.
cc -shared -fPIC "$shim_source" -ldl -o "$workdir/cpuinfo.so"
if LD_PRELOAD="$workdir/cpuinfo.so" LATX_TEST_HOST_FEATURES='Features: lbt_x86' \
    "$emulator" "$workdir/lbt-options" >"$workdir/probe.log" 2>&1; then
    echo 'SKIP: static executable; dynamic cpuinfo override not applicable'
else
    grep -q 'LAT needs LSX' "$workdir/probe.log"
    cache_home="$workdir/cpuinfo"
    mkdir -p "$cache_home/.config"
    printf 'LATX_LBT=1\nLATX_SOFTFPU=0\n' >"$cache_home/.config/latx-x86_64.conf"
    HOME="$cache_home" LD_PRELOAD="$workdir/cpuinfo.so" \
        LATX_TEST_HOST_FEATURES='Features: lsx lasx lbt_arm' \
        LATX_LBT=1 LATX_AOT_GENERATE=1 \
        "$emulator" -latx-lbt 1 "$workdir/lbt-options"
    count=0
    while ! find "$cache_home" -name '*-lbt0-lasx*-fpu2-fast*.aot2' -size +0c | grep -q .; do
        count=$((count + 1))
        test "$count" -lt 10
        sleep 1
    done
fi
echo 'PASS: LBT options, cpuinfo detection, softfpu defaults and AOT loading'
