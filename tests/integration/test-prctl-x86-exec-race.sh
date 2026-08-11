#!/bin/sh
set -eu

emulator=$1
emulator_path=$(realpath "$emulator")
source_file=$2
native_helper_source=$3
shim_source=$4
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
"${CC:-cc}" -O2 -Wall -Wextra -DPRCTL_EXEC_RACE_FAIL \
    "$native_helper_source" -o "$workdir/prctl-native-fail-helper"
"${CC:-cc}" -shared -fPIC -O2 -Wall -Wextra "$shim_source" \
    -o "$workdir/prctl-exec-race-shim.so"
printf '#!%s %s\n' "$emulator_path" "$workdir/prctl-x86-semantics" \
    >"$workdir/prctl-x86-script"
chmod +x "$workdir/prctl-x86-script"
printf '#!%s carrier\nPR378_EXEC_RACE_ORIGINAL\n' \
    "$workdir/prctl-native-env-helper" >"$workdir/prctl-carrier-script"
printf '#!%s carrier\nPR378_EXEC_RACE_REPLACEMENT\n' \
    "$workdir/prctl-native-fail-helper" \
    >"$workdir/prctl-carrier-replacement"
chmod +x "$workdir/prctl-carrier-script" \
    "$workdir/prctl-carrier-replacement"

run_race()
{
    name=$1
    initial=$2
    replacement=$3
    mode=$4
    target="$workdir/$name-target"
    alternate="$workdir/$name-alternate"
    ready="$workdir/$name-ready"
    release="$workdir/$name-release"

    cp "$initial" "$target"
    cp "$replacement" "$alternate"
    env LATX_AOT=0 LATX_KZT=0 \
        LD_PRELOAD="$workdir/prctl-exec-race-shim.so" \
        LATX_EXEC_RACE_TARGET="$target" \
        LATX_EXEC_RACE_READY="$ready" \
        LATX_EXEC_RACE_RELEASE="$release" \
        timeout -s KILL 15 "$emulator" "$workdir/prctl-x86-semantics" \
            "$mode" "$target" &
    pid=$!

    i=0
    while test ! -e "$ready" && test "$i" -lt 100; do
        sleep 0.05
        i=$((i + 1))
    done
    if test ! -e "$ready"; then
        touch "$release"
        wait "$pid" || true
        echo "FAIL: $name exec race did not reach the inspection barrier" >&2
        return 1
    fi
    mv "$target" "$workdir/$name-swap"
    mv "$alternate" "$target"
    mv "$workdir/$name-swap" "$alternate"
    touch "$release"
    if ! wait "$pid"; then
        echo "FAIL: $name exec race used a different file after inspection" >&2
        return 1
    fi
    echo "PASS: $name exec classification and execution used one file"
}

run_race execve-x86-to-native "$workdir/prctl-x86-script" \
    "$workdir/prctl-native-fail-helper" t
run_race execve-native-to-x86 "$workdir/prctl-native-env-helper" \
    "$workdir/prctl-x86-script" t
run_race execve-script-carrier "$workdir/prctl-carrier-script" \
    "$workdir/prctl-carrier-replacement" t
run_race execveat-x86-to-native "$workdir/prctl-x86-script" \
    "$workdir/prctl-native-fail-helper" u
run_race execveat-native-to-x86 "$workdir/prctl-native-env-helper" \
    "$workdir/prctl-x86-script" u
run_race execveat-script-carrier "$workdir/prctl-carrier-script" \
    "$workdir/prctl-carrier-replacement" u
