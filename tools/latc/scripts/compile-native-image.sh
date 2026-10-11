#!/bin/sh
set -eu

module=0
static_tbset=0
while :; do
    case "${1:-}" in
        --module) module=1; shift ;;
        --static-tbset) static_tbset=1; shift ;;
        *) break ;;
    esac
done
if [ "$#" -lt 4 ] || [ "$#" -gt 5 ]; then
    echo "usage: $0 [--module] [--static-tbset] LATC RUNNER X86_GUEST OUTPUT [TBSET]" >&2
    exit 2
fi

latc=$1
runner=$2
guest=$3
output=$4
tbset=${5:-}

unset LATC_NATIVE_STATIC_CHECK
if [ -z "$tbset" ] || [ "$static_tbset" -eq 1 ]; then
    export LATC_NATIVE_STATIC_CHECK=1
fi

case "$output" in
    /*) ;;
    *) output="$(pwd)/$output" ;;
esac
work=$(mktemp -d "${TMPDIR:-/tmp}/latc-native.XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM

missing="$work/missing.tbset"
rm -f "$output" "$missing"
if [ "$module" -eq 1 ]; then
    export LATC_NATIVE_ALLOW_MISSING=1
else
    unset LATC_NATIVE_ALLOW_MISSING
fi
if ! LATC_NATIVE_IMAGE_OUT="$output" LATC_NATIVE_MISSING_OUT="$missing" \
    "$(dirname "$0")/compile-aot.sh" "$latc" "$runner" "$guest" \
    "$work/compile-bundle" "$tbset" >/dev/null; then
    if [ -s "$missing" ]; then
        missing_count=$(wc -l <"$missing")
        echo "latc: native compilation missing $missing_count static targets; first entries:" >&2
        sed -n '1,32p' "$missing" >&2
    else
        echo "latc: native compilation failed without static missing targets" >&2
    fi
    exit 1
fi
if [ ! -s "$output" ]; then
    echo "latc: native compilation produced no image" >&2
    exit 1
fi
if [ "$module" -eq 0 ]; then
    "$latc" mark-native-x86 "$output"
fi
"$latc" inspect-native --json "$output"
