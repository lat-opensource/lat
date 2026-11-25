#!/bin/sh
set -eu
emulator=$(realpath "$1")
source_file=$2
arch=$3
workdir=$(mktemp -d)
trap 'rm -rf "$workdir"' EXIT HUP INT TERM
if [ "$(getconf PAGESIZE)" -le 4096 ]; then
    echo 'SKIP: requires a host with pages larger than 4 KiB'
    exit 77
fi
case "$arch" in
    i386)
        compiler=${I386_CC:-i686-linux-gnu-gcc}
        sysroot=${I386_SYSROOT:-/usr/gnemul/latx-i386}
        ;;
    x86_64)
        compiler=${X86_64_CC:-x86_64-linux-gnu-gcc}
        sysroot=${X86_64_SYSROOT:-/usr/gnemul/latx-x86_64}
        ;;
    *) echo "Unknown guest ABI: $arch" >&2; exit 1 ;;
esac
if ! command -v "$compiler" >/dev/null 2>&1; then
    echo "SKIP: $arch cross compiler is required"
    exit 77
fi
cat > "$workdir/link.c" <<'PROBE'
#include <pthread.h>
int main(void) { return 0; }
PROBE
if ! "$compiler" -O2 -pthread "$workdir/link.c" -o "$workdir/link"; then
    echo "SKIP: compiler cannot link $arch pthread guests"
    exit 77
fi
if ! "$emulator" -L "$sysroot" "$workdir/link"; then
    echo "SKIP: $arch guest runtime is unavailable"
    exit 77
fi
"$compiler" -O2 -pthread "$source_file" -o "$workdir/probe"
for mode in 0 1 4 fork dup nonblock timeout small multi-timeout \
    fcntl-nonblock peek-trunc switch null many reuse bad-timeout \
    readonly-timeout readonly-timeout-data readonly-timeout-nonblock \
    vlen0 vlen0-invalid-timeout \
    vlen0-bad-timeout huge-timeout huge-socket socket-timeout \
    recv-bad-iov recv-bad-buffer mmsg-bad-iov mmsg-bad-buffer \
    mmsg-partial-bad-iov \
    recv-empty-iov recv-empty-iov-trunc recv-empty-iov-peek \
    mmsg-empty-iov mmsg-empty-iov-trunc mmsg-empty-iov-peek \
    recv-zero-iov mmsg-zero-iov \
    recv-flags-reuse mmsg-flags-reuse recv-control-flags mmsg-control-flags \
    socket-timeout-data socket-timeout-call-first socket-timeout-dontwait \
    socket-timeout-nonblock socket-timeout-waitforone rights \
    rights-process exec directory-lock \
    expired-timeout-zero expired-timeout-tiny \
    timeout-snapshot-socket timeout-snapshot-signal; do
    env LATX_AOT=0 LATX_KZT=0 LATX_TEST_EMULATOR="$emulator" \
        LATX_TEST_SYSROOT="$sysroot" timeout 15 "$emulator" \
        -L "$sysroot" "$workdir/probe" "$mode"
done
