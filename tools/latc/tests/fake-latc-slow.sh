#!/bin/sh
set -eu
script_dir=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
real_latc=${LATC_FAKE_REAL:-$script_dir/../build/latc}
if [ "${1:-}" = build-id ]; then
    exec "$real_latc" build-id
fi
if [ -z "${LATC_FAKE_REAL:-}" ]; then
    previous=
    for argument in "$@"; do
        if [ "$previous" = --runner ]; then
            candidate=$(dirname "$argument")/latc
            if [ -x "$candidate" ]; then
                real_latc=$candidate
            fi
            break
        fi
        previous=$argument
    done
fi
sleep 1
exec "$real_latc" "$@"
