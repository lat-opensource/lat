#!/bin/sh
set -eu

emulator=$1
source_file=$2
helper_source=$3
workdir=$(mktemp -d)
helper_pid=
helper_nlink=

cleanup()
{
    if [ -n "$helper_pid" ]; then
        kill -KILL "$helper_pid" 2>/dev/null || true
        wait "$helper_pid" 2>/dev/null || true
    fi
    rm -rf "$workdir"
}
trap cleanup EXIT HUP INT TERM

if ! command -v unshare >/dev/null 2>&1 || ! unshare -Ur true 2>/dev/null; then
    echo "SKIP: unprivileged user namespaces are unavailable"
    exit 77
fi

if command -v clang-19 >/dev/null 2>&1; then
    clang=clang-19
elif command -v clang >/dev/null 2>&1; then
    clang=clang
else
    echo "SKIP: clang is required to build the x86_64 guest"
    exit 77
fi

"$clang" --target=x86_64-linux-gnu -fuse-ld=lld -nostdlib -static \
    -Wl,--build-id=none "$source_file" -o "$workdir/cef-procfs-chroot"

"${CC:-cc}" -Wall -Wextra -Werror -pthread "$helper_source" \
    -o "$workdir/cef-procfs-other-task"
"$workdir/cef-procfs-other-task" &
helper_pid=$!

i=0
while [ "$(stat -c %h "/proc/$helper_pid/task")" -lt 4 ]; do
    if ! kill -0 "$helper_pid" 2>/dev/null || [ "$i" -ge 100 ]; then
        echo "FAIL: native helper did not start two threads" >&2
        exit 1
    fi
    i=$((i + 1))
    sleep 0.05
done
helper_nlink=$(stat -c %h "/proc/$helper_pid/task")

set +e
SBX_USER_NS=1 LATX_AOT=0 LATX_KZT=0 \
    "$emulator" "$workdir/cef-procfs-chroot" "$helper_pid" "$helper_nlink"
ret=$?
set -e

case $ret in
0)
    echo "PASS: procfd hides translator threads after sandbox chroot"
    ;;
30)
    echo "FAIL: opening /proc failed" >&2
    ;;
31)
    echo "FAIL: unshare(CLONE_NEWUSER) failed" >&2
    ;;
32)
    echo "FAIL: sandbox chroot failed" >&2
    ;;
33)
    echo "FAIL: fstatat relative to the saved procfd failed" >&2
    ;;
34)
    echo "FAIL: procfd exposed translator-only threads after chroot" >&2
    ;;
35)
    echo "FAIL: opening /proc/self/task relative to the saved procfd failed" >&2
    ;;
36)
    echo "FAIL: fstat on the saved task fd failed after chroot" >&2
    ;;
37)
    echo "FAIL: fstat on the saved task fd exposed translator-only threads" >&2
    ;;
38)
    echo "FAIL: fstatat(AT_EMPTY_PATH) on the saved task fd failed" >&2
    ;;
39)
    echo "FAIL: fstatat(AT_EMPTY_PATH) exposed translator-only threads" >&2
    ;;
40)
    echo "FAIL: statx(AT_EMPTY_PATH) on the saved task fd failed" >&2
    ;;
41)
    echo "FAIL: statx(AT_EMPTY_PATH) exposed translator-only threads" >&2
    ;;
42)
    echo "FAIL: opening another process task directory failed" >&2
    ;;
44)
    echo "FAIL: invalid native helper task link count" >&2
    ;;
45)
    echo "FAIL: fstat on another process task fd failed after chroot" >&2
    ;;
46)
    echo "FAIL: fstat rewrote another process task count" >&2
    ;;
47)
    echo "FAIL: fstatat(AT_EMPTY_PATH) on another task fd failed" >&2
    ;;
48)
    echo "FAIL: fstatat(AT_EMPTY_PATH) rewrote another task count" >&2
    ;;
49)
    echo "FAIL: statx(AT_EMPTY_PATH) on another task fd failed" >&2
    ;;
50)
    echo "FAIL: statx(AT_EMPTY_PATH) rewrote another task count" >&2
    ;;
51)
    echo "FAIL: statx relative to the saved procfd failed" >&2
    ;;
52)
    echo "FAIL: relative statx exposed translator-only threads" >&2
    ;;
53)
    echo "FAIL: relative statx on another task directory failed" >&2
    ;;
54)
    echo "FAIL: relative statx rewrote another task count" >&2
    ;;
*)
    echo "FAIL: unexpected guest exit status $ret" >&2
    ;;
esac

exit "$ret"
