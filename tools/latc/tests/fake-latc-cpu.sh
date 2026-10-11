#!/bin/sh
if [ "${1:-}" = build-id ]; then
    script_dir=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
    real_latc=${LATC_FAKE_REAL:-$script_dir/../build/latc}
    exec "$real_latc" build-id
fi
while :; do :; done
