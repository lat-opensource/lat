#!/bin/sh
set -eu

if [ "$#" -ne 6 ]; then
    echo "usage: $0 RUNNER GUEST RUN_DIR EXPECTED_DIR WORKDIR INPUT_TIMEOUT" >&2
    exit 2
fi

runner=$1
guest=$2
run_dir=$3
expected_dir=$4
workdir=$5
input_timeout=$6

case_dir="$workdir/old-aot-tu-replay"
home="$case_dir/home"
tmp="$case_dir/tmp"

if [ -e "$case_dir" ]; then
    echo "refusing to reuse existing case directory: $case_dir" >&2
    exit 2
fi
mkdir -p "$case_dir" "$home" "$tmp"

snapshot_cache()
{
    stage=$1
    cache="$home/.cache/latx"
    manifest="$case_dir/$stage.cache.manifest"
    signature="$case_dir/$stage.cache.signature"
    wait_seconds=${CACHE_WAIT_SECONDS:-60}
    remaining=$wait_seconds

    while [ "$remaining" -gt 0 ]; do
        if [ -d "$cache" ] \
            && ! find "$cache" -type f -name '*.aot2.tmp' -print \
                | grep -q .; then
            find "$cache" -type f -name '*.aot2' -size +0c -print \
                | LC_ALL=C sort >"$manifest"
            if [ -s "$manifest" ]; then
                find "$cache" -type f -name '*.aot2' -size +0c \
                    -exec sha256sum -- {} + \
                    | LC_ALL=C sort >"$manifest.sha256"
                cat "$manifest" "$manifest.sha256" >"$signature.next"
                if [ -f "$signature" ] \
                    && cmp -s "$signature" "$signature.next"; then
                    mv "$signature.next" "$signature"
                    echo "$stage: legacy AOT cache stable with $(wc -l <"$manifest") file(s)"
                    return 0
                fi
                mv "$signature.next" "$signature"
            fi
        fi
        remaining=$((remaining - 1))
        if [ "$remaining" -gt 0 ]; then
            sleep 1
        fi
    done

    echo "$stage: legacy AOT cache did not become non-empty and stable within ${wait_seconds}s" >&2
    exit 1
}

report_cache_transition()
{
    before=$1
    after=$2

    if cmp -s "$case_dir/$before.cache.manifest" \
        "$case_dir/$after.cache.manifest"; then
        echo "$after: cache file set unchanged from $before"
    else
        echo "$after: cache file set evolved from $before"
    fi
}

run_guest()
{
    input=$1

    set +e
    env -i \
        PATH=/usr/bin:/bin \
        HOME="$home" \
        TMPDIR="$tmp" \
        LANG=C \
        LC_ALL=C \
        LATX_AOT=1 \
        timeout --signal=TERM --kill-after=5s "$input_timeout" \
        "$runner" "$guest" "$input" 60 \
        >"$case_dir/$input.stdout" \
        2>"$case_dir/$input.stderr"
    rc=$?
    set -e
    printf '%s\n' "$rc" >"$case_dir/$input.rc"
    if [ "$rc" -ne 0 ]; then
        echo "$input failed with rc=$rc" >&2
        exit "$rc"
    fi
    if [ -s "$case_dir/$input.stderr" ]; then
        echo "$input produced stderr:" >&2
        cat "$case_dir/$input.stderr" >&2
        exit 1
    fi
    if ! cmp "$case_dir/$input.stdout" "$expected_dir/$input.out"; then
        echo "$input output mismatch" >&2
        exit 1
    fi
    echo "$input rc=0"
}

# The first two inputs create and merge the legacy AOT cache. The third input
# is the regression: its TU has non-contiguous members in the AOT table, so
# recovery must relocate every member before the code executes.
#
# Capability boundary: this script proves that non-empty legacy AOT cache
# files exist and that replay returns the expected output.  It does not prove
# an AOT cache hit or distinguish AOT execution from JIT fallback; those
# require runner debugging, tracing, or explicit counters.
cd "$run_dir"
run_guest input.source
snapshot_cache input.source
run_guest input.log
snapshot_cache input.log
report_cache_transition input.source input.log
run_guest input.graphic
snapshot_cache input.graphic
report_cache_transition input.log input.graphic

if find "$home/.cache/latx" -type f -name '*.aot2.tmp' -print \
    | grep -q .; then
    echo "legacy AOT cache still has .aot2.tmp files" >&2
    exit 1
fi

echo "test-old-aot-tu-replay: PASS"
