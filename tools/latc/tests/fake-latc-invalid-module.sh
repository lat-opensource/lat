#!/bin/sh
set -eu

if [ "${1:-}" = build-id ]; then
    script_dir=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
    real_latc=${LATC_FAKE_REAL:-$script_dir/../build/latc}
    exec "$real_latc" build-id
fi

output=
while [ "$#" -gt 0 ]; do
    if [ "$1" = -o ] && [ "$#" -gt 1 ]; then
        output=$2
        shift 2
    else
        shift
    fi
done
test -n "$output"
cp /bin/true "$output"
