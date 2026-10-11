#!/bin/sh
set -eu

if [ "$#" -ne 2 ]; then
    echo "usage: $0 FULL_LAT_SOURCE BUILD_DIR" >&2
    exit 2
fi

source_dir=$(cd "$1" && pwd)
build_dir=$2
case "$build_dir" in /*) ;; *) build_dir="$(pwd)/$build_dir";; esac
staging_dir="${build_dir}.source"
script_dir=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
latc_dir=$(dirname "$script_dir")
source_id=$(python3 "$script_dir/compute-aot-v2-build-id.py" "$source_dir" --source-only)

rm -rf "$build_dir" "$staging_dir"
mkdir -p "$staging_dir"
rsync -a --delete --exclude 'build*' "$source_dir/" "$staging_dir/"
touch "$staging_dir/.latc-staging"
printf '%s\n' "$source_id" >"$staging_dir/.latc-source-id"
python3 "$script_dir/prepare-runner-source.py" "$staging_dir"
mkdir -p "$build_dir"

cd "$build_dir"
"$staging_dir/configure" --target-list=x86_64-linux-user --enable-latx \
    --optimize-O1 --disable-docs --disable-tools --disable-plugins \
    --disable-debug-info --disable-werror --without-default-features \
    --disable-gnutls --disable-nettle --disable-gcrypt --disable-curl \
    --disable-bzip2 --disable-lzo --disable-snappy --disable-libssh \
    --disable-slirp --disable-capstone
ninja latx-x86_64 latc latcd
LD_LIBRARY_PATH="$build_dir${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}" \
  ./latx-x86_64 --latc-print-x86-env-offsets >latc-x86-env-offsets.h
test -s latc-x86-env-offsets.h
file latx-x86_64 liblat-aot-runtime.so.2
