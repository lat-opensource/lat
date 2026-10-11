#!/bin/sh
set -eu

emulator=$(readlink -f "$1")
source_file=$(readlink -f "$2")
file_source=$(readlink -f "$3")
tmp_root=${TMPDIR:-${HOME}/tmp}
mkdir -p "$tmp_root"
workdir=$(mktemp -d "$tmp_root/push-pop-sp.XXXXXX")
trap 'rm -rf "$workdir"' EXIT HUP INT TERM

if command -v clang-19 >/dev/null 2>&1; then
    clang=clang-19
elif command -v clang >/dev/null 2>&1; then
    clang=clang
else
    echo "SKIP: clang is required to build the x86_64 guest"
    exit 77
fi

if command -v llvm-objcopy-19 >/dev/null 2>&1; then
    objcopy=llvm-objcopy-19
elif command -v llvm-objcopy >/dev/null 2>&1; then
    objcopy=llvm-objcopy
else
    echo "SKIP: llvm-objcopy is required to build the file-mapped guest code"
    exit 77
fi

"$clang" --target=x86_64-linux-gnu -fuse-ld=lld -nostdlib -static \
    -Wl,--build-id=none "$source_file" -o "$workdir/push-pop-sp"
"$clang" --target=x86_64-linux-gnu -c "$file_source" \
    -o "$workdir/push-pop-sp-file.o"
"$objcopy" -O binary --only-section=.text "$workdir/push-pop-sp-file.o" \
    "$workdir/push-pop-sp-file.bin"

set +e
timeout 10s env LATX_AOT=0 LATX_KZT=0 \
    "$emulator" "$workdir/push-pop-sp" "$workdir/push-pop-sp-file.bin"
ret=$?
set -e

case $ret in
0) ;;
10) echo "FAIL: rt_sigaction failed" >&2; exit "$ret" ;;
11) echo "FAIL: sigaltstack setup failed" >&2; exit "$ret" ;;
20) echo "FAIL: consecutive PUSH/POP values or RSP mismatch" >&2; exit "$ret" ;;
21) echo "FAIL: RSP memory or alias use mismatch" >&2; exit "$ret" ;;
22) echo "FAIL: PUSHF/POPF or ENTER/LEAVE mismatch" >&2; exit "$ret" ;;
23) echo "FAIL: helper boundary did not preserve RSP" >&2; exit "$ret" ;;
24) echo "FAIL: fault handler was not called" >&2; exit "$ret" ;;
25) echo "FAIL: RSP was not restored after the fault" >&2; exit "$ret" ;;
26) echo "FAIL: file-backed code mapping failed" >&2; exit "$ret" ;;
27) echo "FAIL: file-backed fault handler was not called" >&2; exit "$ret" ;;
28)
    echo "FAIL: guard-page PUSH fault handler was not called" >&2
    exit "$ret"
    ;;
29) echo "FAIL: guard-page PUSH setup failed" >&2; exit "$ret" ;;
30) echo "FAIL: wrong signal reached the handler" >&2; exit "$ret" ;;
31) echo "FAIL: fault ucontext contained the wrong RSP" >&2; exit "$ret" ;;
40) echo "FAIL: file-backed PUSH/POP values or RSP mismatch" >&2; exit "$ret" ;;
124) echo "FAIL: PUSH/POP RSP test timed out" >&2; exit "$ret" ;;
*) echo "FAIL: unexpected exit status $ret" >&2; exit "$ret" ;;
esac

echo "PASS: PUSH/POP widths, RSP uses, helper, fault, guard page," \
    "and file mapping"
