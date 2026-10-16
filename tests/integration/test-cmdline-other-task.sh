#!/bin/sh
set -eu

emulator=$1
source_file=$2
workdir=$(mktemp -d)
trap 'rm -rf "$workdir"' EXIT HUP INT TERM

choose_compiler()
{
    cc=${X86_64_CC:-x86_64-linux-gnu-gcc}
    if command -v "$cc" >/dev/null 2>&1; then
        printf 'int main(void){return 0;}\n' >"$workdir/probe.c"
        if "$cc" -m64 -static "$workdir/probe.c" -o "$workdir/probe" \
                2>/dev/null; then
            compiler=$cc
            compiler_mode=gcc
            return 0
        fi
    fi
    for clang in clang-19 clang; do
        command -v "$clang" >/dev/null 2>&1 || continue
        printf 'int main(void){return 0;}\n' >"$workdir/probe.c"
        if "$clang" --target=x86_64-linux-gnu -fuse-ld=lld -static -no-pie \
                "$workdir/probe.c" -o "$workdir/probe" 2>/dev/null; then
            compiler=$clang
            compiler_mode=clang
            return 0
        fi
    done
    return 1
}

if [ -n "${LATX_CMDLINE_GUEST:-}" ]; then
    guest=$LATX_CMDLINE_GUEST
elif ! choose_compiler; then
    echo "SKIP: x86 Linux compiler with static libc is required"
    exit 77
else
    guest=$workdir/cmdline-other-task
    if [ "$compiler_mode" = gcc ]; then
        "$compiler" -m64 -O2 -Wall -Wextra -Werror -static \
            "$source_file" -o "$guest"
    else
        "$compiler" --target=x86_64-linux-gnu -fuse-ld=lld -static -no-pie \
            -O2 -Wall -Wextra "$source_file" -o "$guest"
    fi
fi

find_registration_interpreter()
{
    dir=$1

    for entry in "$dir"/*; do
        [ -f "$entry" ] || continue
        magic=$(sed -n 's/^magic //p' "$entry" 2>/dev/null | head -n 1)
        case $magic in
        7f454c4602*)
            sed -n 's/^interpreter //p' "$entry" 2>/dev/null | head -n 1
            return 0
            ;;
        esac
    done
    return 1
}

run_case()
{
    label=$1
    lookup_dir=$2
    shift 2

    if [ -d "$lookup_dir" ]; then
        registered=$(find_registration_interpreter "$lookup_dir") || registered=
        if [ -n "$registered" ] && [ "$registered" != "$emulator_real" ]; then
            echo "SKIP: $label registration names $registered"
            return
        fi
    fi

    set +e
    env "$@" LATX_AOT=0 LATX_KZT=0 timeout -s KILL 30 \
        "$emulator" "$guest"
    ret=$?
    set -e

    ran=$((ran + 1))
    if [ "$ret" -eq 0 ]; then
        echo "PASS: $label reads another task cmdline"
        return
    fi
    if [ "$ret" -eq 124 ]; then
        echo "FAIL: $label timed out" >&2
    else
        echo "FAIL: $label returned $ret" >&2
    fi
    exit 1
}

emulator_real=$(realpath "$emulator")
ran=0
# LATX_DOCKER points the interpreter lookup at /binfmt_misc, which is
# normally absent outside containers.  The translator must fall back to
# its own executable path instead of failing to filter the command line.
run_case "latx-docker-path" /binfmt_misc LATX_DOCKER=1
run_case "default-path" /proc/sys/fs/binfmt_misc

if [ "$ran" -eq 0 ]; then
    echo "SKIP: no registration names this emulator"
    exit 77
fi
