#!/bin/sh
set -eu

script_dir=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
real_latc=${LATC_FAKE_REAL:-$script_dir/../build/latc}
if [ "${1:-}" = build-id ]; then
    exec "$real_latc" build-id
fi
exit 1
