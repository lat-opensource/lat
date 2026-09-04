#!/bin/sh
set -eu

emulator=$1
source_file=$2
helper_source=$3
workdir=$(mktemp -d)
helper_pid=
helper_nlink=
ordinary_root="$workdir/ordinary"

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
mkdir -p "$ordinary_root/self/task" "$ordinary_root/task"

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
    "$emulator" "$workdir/cef-procfs-chroot" "$helper_pid" "$helper_nlink" \
    "$ordinary_root"
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
55)
    echo "FAIL: opening the ordinary directory root failed" >&2
    ;;
56)
    echo "FAIL: opening the ordinary self/task directory failed" >&2
    ;;
57)
    echo "FAIL: fchdir to the saved procfd failed" >&2
    ;;
58)
    echo "FAIL: fstatat relative to AT_FDCWD failed" >&2
    ;;
59)
    echo "FAIL: relative AT_FDCWD fstatat exposed translator threads" >&2
    ;;
60)
    echo "FAIL: statx relative to AT_FDCWD failed" >&2
    ;;
61)
    echo "FAIL: relative AT_FDCWD statx exposed translator threads" >&2
    ;;
62)
    echo "FAIL: relative AT_FDCWD fstatat on another task failed" >&2
    ;;
63)
    echo "FAIL: relative AT_FDCWD fstatat rewrote another task" >&2
    ;;
64)
    echo "FAIL: relative AT_FDCWD statx on another task failed" >&2
    ;;
65)
    echo "FAIL: relative AT_FDCWD statx rewrote another task" >&2
    ;;
66)
    echo "FAIL: fchdir to the saved self-task fd failed" >&2
    ;;
67)
    echo "FAIL: AT_EMPTY_PATH fstatat through AT_FDCWD failed" >&2
    ;;
68)
    echo "FAIL: AT_FDCWD empty-path fstatat exposed translator threads" >&2
    ;;
69)
    echo "FAIL: AT_EMPTY_PATH statx through AT_FDCWD failed" >&2
    ;;
70)
    echo "FAIL: AT_FDCWD empty-path statx exposed translator threads" >&2
    ;;
71)
    echo "FAIL: fchdir to another process task fd failed" >&2
    ;;
72)
    echo "FAIL: AT_FDCWD empty-path fstatat on another task failed" >&2
    ;;
73)
    echo "FAIL: AT_FDCWD empty-path fstatat rewrote another task" >&2
    ;;
74)
    echo "FAIL: AT_FDCWD empty-path statx on another task failed" >&2
    ;;
75)
    echo "FAIL: AT_FDCWD empty-path statx rewrote another task" >&2
    ;;
76)
    echo "FAIL: fchdir to the ordinary directory root failed" >&2
    ;;
77)
    echo "FAIL: relative fstatat on an ordinary directory failed" >&2
    ;;
78)
    echo "FAIL: relative AT_FDCWD fstatat rewrote an ordinary directory" >&2
    ;;
79)
    echo "FAIL: relative statx on an ordinary directory failed" >&2
    ;;
80)
    echo "FAIL: relative AT_FDCWD statx rewrote an ordinary directory" >&2
    ;;
81)
    echo "FAIL: fchdir to the ordinary self/task directory failed" >&2
    ;;
82)
    echo "FAIL: empty-path fstatat on an ordinary directory failed" >&2
    ;;
83)
    echo "FAIL: empty-path AT_FDCWD fstatat rewrote an ordinary directory" >&2
    ;;
84)
    echo "FAIL: empty-path statx on an ordinary directory failed" >&2
    ;;
85)
    echo "FAIL: empty-path AT_FDCWD statx rewrote an ordinary directory" >&2
    ;;
86)
    echo "FAIL: opening /proc/self relative to the saved procfd failed" >&2
    ;;
87)
    echo "FAIL: fchdir to the saved /proc/self fd failed" >&2
    ;;
88)
    echo "FAIL: fstatat of task relative to /proc/self failed" >&2
    ;;
89)
    echo "FAIL: /proc/self-relative fstatat exposed translator threads" >&2
    ;;
90)
    echo "FAIL: statx of task relative to /proc/self failed" >&2
    ;;
91)
    echo "FAIL: /proc/self-relative statx exposed translator threads" >&2
    ;;
92)
    echo "FAIL: fstatat of dot from /proc/self/task failed" >&2
    ;;
93)
    echo "FAIL: dot-relative fstatat exposed translator threads" >&2
    ;;
94)
    echo "FAIL: statx of dot from /proc/self/task failed" >&2
    ;;
95)
    echo "FAIL: dot-relative statx exposed translator threads" >&2
    ;;
96)
    echo "FAIL: cloning the cwd-racing guest thread failed" >&2
    ;;
97)
    echo "FAIL: fstatat failed while another thread changed cwd" >&2
    ;;
98)
    echo "FAIL: racing fstatat returned an invalid task link count" >&2
    ;;
99)
    echo "FAIL: statx failed while another thread changed cwd" >&2
    ;;
100)
    echo "FAIL: racing statx returned an invalid task link count" >&2
    ;;
101)
    echo "FAIL: the cwd-racing guest thread could not change cwd" >&2
    ;;
102)
    echo "FAIL: cwd race missed a task-directory case" >&2
    ;;
103)
    echo "FAIL: fstatat failed while another thread replaced dirfd" >&2
    ;;
104)
    echo "FAIL: racing dirfd fstatat returned an invalid link count" >&2
    ;;
105)
    echo "FAIL: statx failed while another thread replaced dirfd" >&2
    ;;
106)
    echo "FAIL: racing dirfd statx returned an invalid link count" >&2
    ;;
107)
    echo "FAIL: duplicating the race dirfd failed" >&2
    ;;
108)
    echo "FAIL: the racing guest thread could not replace dirfd" >&2
    ;;
109)
    echo "FAIL: fstat on /proc/self before the race failed" >&2
    ;;
110)
    echo "FAIL: fstatat failed while another thread changed pathname" >&2
    ;;
111)
    echo "FAIL: racing pathname fstatat returned an invalid link count" >&2
    ;;
112)
    echo "FAIL: statx failed while another thread changed pathname" >&2
    ;;
113)
    echo "FAIL: racing pathname statx returned an invalid link count" >&2
    ;;
114)
    echo "FAIL: pathname race missed one of the valid target paths" >&2
    ;;
115)
    echo "FAIL: opening the host root failed" >&2
    ;;
116)
    echo "FAIL: opening the sandbox root failed" >&2
    ;;
117)
    echo "FAIL: cloning the root-racing guest thread failed" >&2
    ;;
118)
    echo "FAIL: stat returned an unexpected error during root changes" >&2
    ;;
119)
    echo "FAIL: racing stat exposed translator-only threads" >&2
    ;;
120)
    echo "FAIL: lstat returned an unexpected error during root changes" >&2
    ;;
121)
    echo "FAIL: racing lstat exposed translator-only threads" >&2
    ;;
122)
    echo "FAIL: the root-racing guest thread could not change root" >&2
    ;;
123)
    echo "FAIL: root race did not exercise both root views" >&2
    ;;
*)
    echo "FAIL: unexpected guest exit status $ret" >&2
    ;;
esac

exit "$ret"
