TBL_LIST="\
arch/x86/entry/syscalls/syscall_32.tbl,linux-user/i386/syscall_32.tbl \
arch/x86/entry/syscalls/syscall_64.tbl,linux-user/x86_64/syscall_64.tbl\
"

linux="$1"
output="$2"

if [ -z "$linux" ] || ! [ -d "$linux" ]; then
    cat << EOF
usage: update-syscalltbl.sh LINUX_PATH [OUTPUT_PATH]

LINUX_PATH      Linux kernel directory to obtain the syscall.tbl from
OUTPUT_PATH     output directory, usually the qemu source tree (default: $PWD)
EOF
    exit 1
fi

if [ -z "$output" ]; then
    output="$PWD"
fi

for entry in $TBL_LIST; do
    OFS="$IFS"
    IFS=,
    set $entry
    src=$1
    dst=$2
    IFS="$OFS"
    if ! cp "$linux/$src" "$output/$dst" ; then
        echo "Cannot copy $linux/$src to $output/$dst" 1>&2
        exit 1
    fi
done
