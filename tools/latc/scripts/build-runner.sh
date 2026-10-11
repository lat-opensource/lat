#!/bin/sh
set -eu

if [ "$#" -ne 2 ]; then
    echo "usage: $0 FULL_LAT_SOURCE BUILD_DIR" >&2
    exit 2
fi

source_dir=$(cd "$1" && pwd)
build_dir=$2
staging_dir="${build_dir}.source"
rm -rf "$build_dir" "$staging_dir"
mkdir -p "$staging_dir"
rsync -a --delete --exclude 'build*' "$source_dir/" "$staging_dir/"
touch "$staging_dir/.latc-staging"
python3 "$(dirname "$0")/compute-aot-v2-build-id.py" "$source_dir" \
    --source-only >"$staging_dir/.latc-source-id"
python3 "$(dirname "$0")/prepare-runner-source.py" \
    --without-aot-v2 "$staging_dir"
mkdir -p "$build_dir"
cd "$build_dir"
"$staging_dir/configure" --target-list=x86_64-linux-user --enable-latx \
    --optimize-O1 --disable-docs --disable-tools --disable-plugins \
    --disable-debug-info --disable-werror --static --without-default-features \
    --disable-gnutls --disable-nettle --disable-gcrypt --disable-curl \
    --disable-bzip2 --disable-lzo --disable-snappy --disable-libssh \
    --disable-slirp --disable-capstone --extra-ldflags=-ldl
ninja latx-x86_64
./latx-x86_64 --latc-print-x86-env-offsets >latc-x86-env-offsets.h
test -s latc-x86-env-offsets.h
file latx-x86_64
