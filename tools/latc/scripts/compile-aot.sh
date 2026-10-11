#!/bin/sh
set -eu

if [ "$#" -lt 4 ] || [ "$#" -gt 5 ]; then
    echo "usage: $0 LATC STATIC_RUNNER X86_GUEST OUTPUT [TBSET]" >&2
    exit 2
fi

latc=$1
runner=$2
guest=$3
output=$4
tbset=${5:-}
work=$(mktemp -d "${TMPDIR:-/tmp}/latc-aot.XXXXXX")
named_guest="$work/x86-guest"

cleanup()
{
    rm -rf "$work"
}
trap cleanup EXIT HUP INT TERM

timing=${LATC_COMPILE_TIMING:-0}
if [ "$timing" -eq 1 ]; then
    timing_start=$(date +%s%N)
    timing_last=$timing_start
fi

report_timing()
{
    [ "$timing" -eq 1 ] || return 0
    timing_now=$(date +%s%N)
    echo "latc: compile timing $1_ms=$(((timing_now - timing_last) / 1000000))" >&2
    timing_last=$timing_now
}

if [ -z "$tbset" ]; then
    tbset="$work/static.tbset"
    "$latc" emit-static-tbset "$guest" -o "$tbset" >/dev/null
fi

if [ -n "$tbset" ]; then
    "$latc" compile "$guest" -o "$work/stage1.la64" --runner "$runner" \
        --tbset "$tbset" --tbset-ignore-outside-exec >/dev/null
else
    "$latc" compile "$guest" -o "$work/stage1.la64" --runner "$runner" \
        >/dev/null
fi
report_timing stage1_bundle
HOME="$work/home" LATX_AOT=1 LATX_AOT_V2=0 LATX_AOT_V2_AUTOSTART=0 LATC_EMIT_AOT=1 \
    LATC_NAMED_GUEST="$named_guest" "$work/stage1.la64"
report_timing translate_export
if [ -n "${LATC_NATIVE_IMAGE_OUT:-}" ]; then
    if [ ! -s "$LATC_NATIVE_IMAGE_OUT" ]; then
        echo "latc: LAT did not produce a native image" >&2
        exit 1
    fi
    if [ "$timing" -eq 1 ]; then
        timing_now=$(date +%s%N)
        echo "latc: compile timing total_ms=$(((timing_now - timing_start) / 1000000))" >&2
    fi
    exit 0
fi
aot=$(find "$work/home/.cache/latx" -type f -name 'v2-*.aot2' \
    -size +0c -print -quit)
if [ -z "$aot" ]; then
    echo "latc: LAT did not produce an AOT file" >&2
    exit 1
fi

if [ -n "$tbset" ]; then
    "$latc" compile "$guest" -o "$output" --runner "$runner" \
        --tbset "$tbset" --tbset-ignore-outside-exec --aot "$aot"
else
    "$latc" compile "$guest" -o "$output" --runner "$runner" --aot "$aot"
fi
report_timing final_bundle
"$latc" inspect --json "$output"
report_timing inspect
if [ "$timing" -eq 1 ]; then
    timing_now=$(date +%s%N)
    echo "latc: compile timing total_ms=$(((timing_now - timing_start) / 1000000))" >&2
fi
