#!/bin/sh
set -eu

emulator=$1
emulator_path=$(realpath "$emulator")
source_file=$2
native_helper_source=$3
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

"$clang" --target=x86_64-linux-gnu -fuse-ld=lld -nostdlib -static -no-pie \
    -O2 -ffreestanding -fno-builtin -fno-stack-protector \
    -Wl,--build-id=none "$source_file" -o "$workdir/prctl-x86-semantics"
"${CC:-cc}" -O2 -Wall -Wextra "$native_helper_source" \
    -o "$workdir/prctl-native-env-helper"
mkdir "$workdir/relative"
printf '#!%s %s\n' "$emulator_path" "$workdir/prctl-x86-semantics" \
    >"$workdir/relative/state-script"
chmod +x "$workdir/relative/state-script"

run_case()
{
    case_name=$1
    shift

    set +e
    env LATX_AOT=0 LATX_KZT=0 timeout -s KILL 10 \
        "$emulator" "$workdir/prctl-x86-semantics" "$case_name" "$@"
    ret=$?
    set -e

    if [ "$ret" -eq 0 ]; then
        echo "PASS: $case_name prctl semantics"
        return
    fi
    if [ "$ret" -eq 124 ]; then
        echo "FAIL: $case_name prctl semantics timed out" >&2
    else
        echo "FAIL: $case_name prctl semantics returned $ret" >&2
    fi
    exit "$ret"
}

run_case p
if [ "$(getconf PAGESIZE)" -gt 4096 ]; then
    run_case v h
else
    run_case v
fi
run_case m
run_case r
run_case s
run_case f
run_case i
run_case e "$workdir/prctl-native-env-helper"
run_case a "$workdir/prctl-native-env-helper"
run_case d "$workdir/relative"
run_case q
run_case o
run_case n
run_case j
