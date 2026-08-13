#!/bin/sh
set -eu

emulator=$1
source_file=$2
payload=$3
webhelper=$4
workdir=$(mktemp -d)
trap 'rm -rf "$workdir"' EXIT HUP INT TERM

if command -v clang-19 >/dev/null 2>&1; then
    clang=clang-19
elif command -v clang >/dev/null 2>&1; then
    clang=clang
else
    echo "SKIP: clang is required to build the x86_64 guest"
    exit 77
fi

if ! "$clang" --target=x86_64-linux-gnu -fuse-ld=lld -nostdlib -static \
    -Wl,--build-id=none "$source_file" -o "$workdir/pressure-vessel-wrap"; then
    echo "SKIP: clang with an x86_64 target and LLD is required"
    exit 77
fi

runtime="$workdir/runtime"
app_lib="$workdir/app-lib"
mkdir -p "$runtime/files/lib/x86_64-linux-gnu" "$runtime/files/share" "$app_lib"

expected_ld="LD_LIBRARY_PATH=$runtime/files/lib/x86_64-linux-gnu:$runtime/files/lib:$app_lib"
expected_path='PATH=/test/path'
expected_xdg="XDG_DATA_DIRS=$runtime/files/share:/test/data"
expected_preload='LD_PRELOAD=/first-preload:/second-preload /third-preload'

for kzt in 0 1; do
    output=$(unset LD_PRELOAD LD_LIBRARY_PATH
        LATX_KZT="$kzt" \
        PRESSURE_VESSEL_RUNTIME_BASE="$workdir" \
        PRESSURE_VESSEL_RUNTIME=runtime \
        "$emulator" "$workdir/pressure-vessel-wrap" \
        --env-if-host=PRESSURE_VESSEL_APP_LD_LIBRARY_PATH="$app_lib" \
        --env-if-host PATH=/test/path \
        --env-if-host XDG_DATA_DIRS=/test/data \
        --ld-preload=/first-preload \
        --ld-preloads '/second-preload /third-preload' \
        -- "$payload" 2>/dev/null)

    printf '%s\n' "$output" | grep -Fx "$expected_ld"
    printf '%s\n' "$output" | grep -Fx "$expected_path"
    printf '%s\n' "$output" | grep -Fx "$expected_xdg"
    printf '%s\n' "$output" | grep -Fx "$expected_preload"
done

output=$(unset LD_PRELOAD LD_LIBRARY_PATH
    LATX_KZT=0 \
    PRESSURE_VESSEL_RUNTIME_BASE="$workdir" \
    PRESSURE_VESSEL_RUNTIME=runtime \
    "$emulator" "$workdir/pressure-vessel-wrap" \
    --ld-preload=/first-preload \
    --env-if-host=LD_PRELOAD=/host-preload \
    -- "$payload" 2>/dev/null)
printf '%s\n' "$output" | grep -Fx 'LD_PRELOAD=/host-preload'

set +e
unset LD_PRELOAD LD_LIBRARY_PATH
LATX_KZT=0 \
    PRESSURE_VESSEL_RUNTIME_BASE="$workdir" \
    PRESSURE_VESSEL_RUNTIME=runtime \
    "$emulator" "$workdir/pressure-vessel-wrap" \
    --launcher -- "$payload"
launcher_status=$?

unset LD_PRELOAD LD_LIBRARY_PATH
LATX_KZT=0 \
    PRESSURE_VESSEL_RUNTIME_BASE="$workdir/missing" \
    PRESSURE_VESSEL_RUNTIME=runtime \
    "$emulator" "$workdir/pressure-vessel-wrap" \
    -- "$payload"
missing_runtime_status=$?

unset LD_PRELOAD LD_LIBRARY_PATH
LATX_KZT=0 \
    PATH=/webhelper-original \
    "$emulator" "$workdir/pressure-vessel-wrap" \
    --env-if-host=PATH=/webhelper-replaced \
    -- "$webhelper"
webhelper_status=$?
set -e

test "$launcher_status" -eq 91
test "$missing_runtime_status" -eq 91
test "$webhelper_status" -eq 0

echo "PASS: pressure-vessel direct payload and fallback paths"
