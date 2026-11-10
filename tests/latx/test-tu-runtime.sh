#!/bin/bash
# SPDX-License-Identifier: GPL-2.0-only
# Build a guest only when its integration test is explicitly executed.
set -euo pipefail
binary=$(realpath "$1")
bits=$2
kind=$3
tests=$(dirname "$(realpath "$0")")
# AOT and TU share the stable/low-memory build gates. Check the compiled
# interface before building a guest or waiting for an unavailable AOT cache.
help=$("$binary" -help)
if [[ "$kind" != entry-signal ]] &&
        ! grep -q '^-latx-aot[[:space:]]' <<<"$help"; then
    echo 'SKIP: cross-page TU requires an AOT/TU-enabled build'
    exit 77
fi
case "$kind" in
    boundary|mode-cache) sources=(tu-boundary.S); name=tu-boundary ;;
    lifecycle) sources=(tu-lifecycle.c tu-lifecycle-code.S tu-lifecycle.h)
               name=tu-lifecycle ;;
    signal-fault)
        sources=(tu-signal-fault.c tu-signal-fault.S tu-signal-fault.h)
        name=tu-signal-fault ;;
    hras-call) sources=(tu-hras-call.S); name=tu-hras-call ;;
    entry-signal) sources=(entry-signal.c entry-signal.h); name=entry-signal ;;
    late-thunk) sources=(tu-late-thunk.S); name=tu-late-thunk ;;
    *) exit 2 ;;
esac
if [[ "$kind" == boundary || "$kind" == late-thunk ||
      "$kind" == entry-signal ]]; then
    command -v gdb >/dev/null || { echo 'SKIP: GDB required'; exit 77; }
    if ! gdb -nx -q -batch \
            -ex "file $binary" \
            -ex 'ptype TranslationBlock' 2>/dev/null |
            grep -q 'type = struct TranslationBlock'; then
        echo 'SKIP: GDB requires a translator build with debug types'
        exit 77
    fi
fi
work=$(mktemp -d "${TMPDIR:-/tmp}/latu-tu-guest.XXXXXX")
trap 'rm -rf "$work"' EXIT
if [[ -n ${LATX_TU_GUEST_DIR:-} ]]; then
    artifacts=$(realpath "$LATX_TU_GUEST_DIR")
    guest="$artifacts/$name$bits"
    # Require identity for both the selected fixture and its current sources.
    for source in "${sources[@]}"; do
        line=$(awk -v name="$source" '$2 == name {print}' \
            "$artifacts/sources.sha256")
        [[ -n "$line" ]] || { echo "missing source hash: $source" >&2; exit 1; }
        (cd "$tests"; printf '%s\n' "$line" | sha256sum -c -)
    done
    line=$(awk -v name="$name$bits" '$2 == name {print}' \
        "$artifacts/artifacts.sha256")
    [[ -n "$line" ]] || { echo "missing guest hash: $guest" >&2; exit 1; }
    (cd "$artifacts"; printf '%s\n' "$line" | sha256sum -c -)
else
    compiler=${LATX_TU_TEST_CC:-}
    if [[ -z "$compiler" ]]; then
        if [[ $(uname -m) == x86_64 ]]; then
            compiler=${CC:-cc}
        else
            if [[ "$bits" == 32 ]]; then
                candidates=(
                    i686-linux-gnu-gcc
                    i386-linux-gnu-gcc
                    x86_64-linux-gnu-gcc
                )
            else
                candidates=(x86_64-linux-gnu-gcc)
            fi
            for candidate in "${candidates[@]}"; do
                if command -v "$candidate" >/dev/null 2>&1; then
                    compiler=$candidate
                    break
                fi
            done
        fi
        if [[ -z "$compiler" ]]; then
            echo 'SKIP: need x86 compiler or LATX_TU_GUEST_DIR'
            exit 77
        fi
    fi
    guest="$work/$name$bits"
    args=(-m"$bits" -O2 -g -no-pie -UNDEBUG)
    if [[ "$kind" == lifecycle || "$kind" == signal-fault ||
          "$kind" == entry-signal ]]; then
        # Static fixtures also work when the host guest sysroot is older.
        args+=(-pthread -static)
    else
        args+=(-nostdlib -static)
    fi
    printf '%s\n' 'int main(void) { return 0; }' >"$work/probe.c"
    if ! "$compiler" "${args[@]}" "$work/probe.c" -o "$work/probe" \
            >"$work/probe.log" 2>&1; then
        echo "SKIP: compiler cannot link the required $bits-bit x86 guest"
        cat "$work/probe.log"
        exit 77
    fi
    inputs=()
    for source in "${sources[@]}"; do
        [[ "$source" == *.h ]] || inputs+=("$tests/$source")
    done
    "$compiler" "${args[@]}" "${inputs[@]}" -o "$guest"
fi
case "$bits" in 32) arch='Intel 80386';; 64) arch=x86-64;; *) exit 2;; esac
file "$guest" | grep -q "ELF $bits-bit.*$arch"
# All result/cache directories survive in TMPDIR for debugging and evidence.
if [[ "$kind" == boundary || "$kind" == mode-cache ]]; then
    bash "$tests/run-tu-$kind.sh" "$(dirname "$binary")" "$guest"
else
    bash "$tests/run-tu-$kind.sh" "$binary" "$guest"
fi
