#!/bin/sh
set -eu
emulator=$1
source_file=$2
arch=$3
workdir=$(mktemp -d)
trap 'rm -rf "$workdir"' EXIT HUP INT TERM

if [ -n "${LATX_SHARED_MMAP_OPTIN_FIXTURE_DIR:-}" ]; then
    guest="$LATX_SHARED_MMAP_OPTIN_FIXTURE_DIR/shared-mmap-optin-$arch"
    expected=$(cat "$LATX_SHARED_MMAP_OPTIN_FIXTURE_DIR/source.sha256")
    actual=$(sha256sum "$source_file" | cut -d ' ' -f 1)
    test "$actual" = "$expected" || {
        echo 'FAIL: prebuilt fixture source mismatch' >&2
        exit 1
    }
else
    if command -v clang-19 >/dev/null 2>&1; then
        compiler=clang-19
    elif command -v clang >/dev/null 2>&1; then
        compiler=clang
    else
        echo 'SKIP: clang/lld or verified prebuilt fixtures required'
        exit 77
    fi
    guest="$workdir/guest"
    "$compiler" --target="$arch-linux-gnu" -fuse-ld=lld \
        -O2 -fomit-frame-pointer -fno-pie -fno-stack-protector \
        -ffreestanding -fno-builtin -nostdlib -static -no-pie \
        "$source_file" -o "$guest"
fi

if [ "$arch" = i386 ] && [ "$(getconf PAGESIZE)" -gt 4096 ]; then
    set +e
    LATX_AOT=0 LATX_KZT=0 LATX_I386_SHARED_MMAP_SHADOW=0 \
        timeout -s KILL 10 "$emulator" "$guest" a
    ret=$?
    set -e
    test "$ret" -eq 1 || {
        echo "FAIL: i386 opt-out changed legacy behavior (exit $ret)" >&2
        exit 1
    }
fi

# x86_64 ignores the i386-only variable and keeps its existing mapping policy.
for item in a:misaligned b:fresh-fragments c:aligned d:fd-zero \
            e:shadow-code-fetch f:fork-sharing; do
    code=${item%%:*}
    LATX_AOT=0 LATX_KZT=0 LATX_I386_SHARED_MMAP_SHADOW=1 \
        timeout -s KILL 10 "$emulator" "$guest" "$code"
    echo "PASS: $arch ${item#*:}"
done
