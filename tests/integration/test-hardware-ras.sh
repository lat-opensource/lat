#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-only
set -eu

emulator=$(readlink -f "$1")
source_file=$(readlink -f "$2")
bits=$3
if ! command -v python3 >/dev/null; then
    echo 'SKIP: python3 is required to wait for detached AOT generation'
    exit 77
fi
workdir=$(mktemp -d)
cleanup() {
    status=$?
    if [ "$status" = 0 ]; then
        rm -rf "$workdir"
    else
        echo "RAS failure evidence: $workdir" >&2
    fi
}
trap cleanup EXIT
trap 'exit 1' HUP INT TERM

# A prebuilt static guest allows the same runner on hosts without an x86
# glibc cross toolchain. Its compilation is a separate, recorded test step.
guest=${LATX_HRAS_TEST_GUEST:-}
if [ -z "$guest" ]; then
    compiler=${LATX_HRAS_TEST_CC:-}
    if [ -z "$compiler" ]; then
        if [ "$(uname -m)" = x86_64 ]; then
            compiler=${CC:-cc}
        elif command -v x86_64-linux-gnu-gcc >/dev/null 2>&1; then
            compiler=x86_64-linux-gnu-gcc
        else
            echo 'SKIP: need an x86 glibc compiler or LATX_HRAS_TEST_GUEST'
            exit 77
        fi
    fi
    printf '%s\n' 'int main(void) { return 0; }' >"$workdir/probe.c"
    if ! "$compiler" -m"$bits" -static -pthread "$workdir/probe.c" \
            -o "$workdir/probe" >"$workdir/probe.log" 2>&1; then
        echo "SKIP: compiler cannot link a static $bits-bit x86 pthread guest"
        cat "$workdir/probe.log"
        exit 77
    fi
    guest=$workdir/guest
    stress_cflags=
    if [ "${LATX_HRAS_TEST_STRESS:-0}" = 1 ]; then
        stress_cflags=-DLATX_HRAS_STRESS
    fi
    "$compiler" -m"$bits" -O2 -g -fno-optimize-sibling-calls -static \
        -pthread $stress_cflags "$source_file" -o "$guest"
fi
guest=$(readlink -f "$guest")
guest_type=$(file "$guest")
if ! printf '%s\n' "$guest_type" |
        grep -q "ELF $bits-bit.*\(x86-64\|Intel 80386\)"; then
    echo "FAIL: expected a $bits-bit x86 guest: $guest" >&2
    exit 1
fi
if ! printf '%s\n' "$guest_type" |
        grep -Eq 'statically linked|static-pie linked'; then
    echo 'SKIP: compiler did not produce the required static guest'
    exit 77
fi

help=$("$emulator" -help)
if ! printf '%s\n' "$help" | grep -q 'latx-hras '; then
    echo 'SKIP: hardware return hints are unavailable in this build'
    exit 77
fi
if ! command -v nm >/dev/null ||
        ! nm -a "$emulator" 2>/dev/null |
            grep -F 'hras_test_marker_init' >/dev/null; then
    echo 'SKIP: HRAS marker checks require a CONFIG_LATX_DEBUG build'
    exit 77
fi

# AOT generation detaches after the guest exits. Waiting on a lock alone can
# race before that worker opens the lock, so wait for the isolated HOME owner.
wait_generation() {
    python3 - "$1" <<'PYWAIT'
import os
import pathlib
import sys
import time

home = ("HOME=" + sys.argv[1]).encode()
deadline = time.monotonic() + 60
while True:
    active = False
    for proc in pathlib.Path("/proc").iterdir():
        if not proc.name.isdigit():
            continue
        try:
            if proc.stat().st_uid == os.getuid() and home in (
                    proc / "environ").read_bytes().split(b"\0"):
                active = True
                break
        except OSError:
            pass
    if not active:
        break
    if time.monotonic() >= deadline:
        raise SystemExit("FAIL: detached AOT generator did not finish")
    time.sleep(0.05)
PYWAIT
}

fast_jmpcache_hash_modes=0
if printf '%s\n' "$help" | grep -q 'latx-fast-jmpcache-hash '; then
    fast_jmpcache_hash_modes='0 1'
fi

# HRAS depends on TU, not AOT. An AOT-disabled run must not be rejected, and
# a future JIT TU path can use the same runtime state. JRRA disables HRAS
# instead of terminating the guest.
fallback_case=0
for fast_jmpcache_hash in $fast_jmpcache_hash_modes; do
    for options in 'LATX_AOT=0' 'LATX_TU=0' 'LATX_JRRA=1' \
                   'LATX_SOFTFPU=1' 'LATX_SOFTFPU=2'; do
        fallback_case=$((fallback_case + 1))
        fallback_home="$workdir/fallback-home-$fallback_case"
        mkdir -p "$fallback_home"
        rm -f "$workdir/fallback-hras"
        timeout 90 env HOME="$fallback_home" LATX_HRAS=1 LATX_AOT=2 \
            LATX_AOT_GENERATE=0 LATX_TU=1 LATX_KZT=0 \
            LATX_HRAS_TEST_SKIP_PAGE_END=1 \
            LATX_JRRA=0 LATX_JRRA_STACK=0 \
            LATX_FAST_JMPCACHE_HASH="$fast_jmpcache_hash" \
            LATX_TEST_HRAS_MARKER="$workdir/fallback-hras" \
            "$options" "$emulator" "$guest" >"$workdir/fallback" 2>&1
        grep -q '^PASS:' "$workdir/fallback"
        # These cases check that HRAS falls back cleanly. Positive HRA
        # marker coverage is the AOT matrix below.
        if [ -e "$workdir/fallback-hras" ]; then
            echo "FAIL: HRAS hints emitted with $options" >&2
            exit 1
        fi
    done
done
for aot_mode in ${LATX_HRAS_TEST_AOT_MODES:-1 2}; do
    for fast_jmpcache_hash in $fast_jmpcache_hash_modes; do
        for reload in 0 1; do
            for hras in 0 1; do
                suffix=$aot_mode-$fast_jmpcache_hash-$reload-$hras
                home="$workdir/home-$suffix"
                mkdir -p "$home"
                for run in 1 2 3 4; do
                    generate=1
                    if [ "$run" = 4 ]; then
                        generate=0
                        rm -f "$home/loaded"
                    fi
                    rm -f "$home/hras"
                    timeout 90 env HOME="$home" LATX_AOT="$aot_mode" \
                        LATX_AOT_GENERATE="$generate" \
                        LATX_KZT=0 LATX_JRRA=0 LATX_JRRA_STACK=0 \
                        LATX_HRAS="$hras" \
                        LATX_TU=1 LATX_SOFTFPU=0 \
                        LATX_FAST_JMPCACHE_HASH="$fast_jmpcache_hash" \
                        LATX_SMC_RELOAD="$reload" \
                        LATX_TEST_AOT_LOAD_MARKER="$home/loaded" \
                        LATX_TEST_HRAS_MARKER="$home/hras" \
                        "$emulator" "$guest"
                    if [ "$generate" = 1 ]; then
                        wait_generation "$home"
                    fi
                    if [ "$run" = 4 ]; then
                        if [ "$hras" = 1 ]; then
                            if ! grep -qx call "$home/hras" ||
                                    ! grep -qx ret "$home/hras"; then
                                echo 'FAIL: missing HRAS CALL/RET hints' >&2
                                exit 1
                            fi
                        elif [ -e "$home/hras" ]; then
                            echo 'FAIL: HRAS hints while disabled' >&2
                            exit 1
                        fi
                    fi
                done
                grep -Fx "$guest" "$home/loaded" >/dev/null
            done
        done
    done
done
echo "PASS: $bits-bit hardware RAS AOT and fallback matrix"
