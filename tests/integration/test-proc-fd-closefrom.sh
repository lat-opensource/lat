#!/bin/sh
set -eu
emulator=$1
source_file=$2
abi=$3
workdir=$(mktemp -d)
trap 'rm -rf "$workdir"' EXIT HUP INT TERM
if command -v clang-19 >/dev/null 2>&1; then
    clang=clang-19
elif command -v clang >/dev/null 2>&1; then
    clang=clang
else
    echo "SKIP: clang is required"
    exit 77
fi
"$clang" --target="$abi-linux-gnu" -fuse-ld=lld -nostdlib -static -no-pie \
    -O2 -ffreestanding -fno-builtin -fno-stack-protector \
    -Wl,--build-id=none "$source_file" -o "$workdir/guest"
mkdir "$workdir/ordinary"
touch "$workdir/ordinary/4"
ln -s /proc/self/fd "$workdir/fd-alias"
for syscall in 64 legacy; do
    env LATX_AOT=0 LATX_KZT=0 timeout -k 2 10 \
        "$emulator" "$workdir/guest" "$syscall" large /proc/self/fd errors
    echo "PASS: $abi $syscall errno"
    env LATX_AOT=0 LATX_KZT=0 timeout -k 2 10 \
        "$emulator" "$workdir/guest" "$syscall" large /proc/self/fd parent
    echo "PASS: $abi $syscall parent directory remains visible"
    for buffer in small large; do
        for directory in /proc/self/fd /proc/thread-self/fd \
                         "$workdir/fd-alias"; do
            env LATX_AOT=0 LATX_KZT=0 timeout -k 2 10 \
                "$emulator" "$workdir/guest" "$syscall" "$buffer" \
                "$directory" close
            echo "PASS: $abi $syscall $buffer close-and-rewind $directory"
        done
        env LATX_AOT=0 LATX_KZT=0 timeout -k 2 10 \
            "$emulator" "$workdir/guest" "$syscall" "$buffer" \
            "$workdir/ordinary" directory
        echo "PASS: $abi $syscall $buffer ordinary directory"
    done
done
