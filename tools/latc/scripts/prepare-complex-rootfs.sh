#!/bin/sh
set -eu

if [ "$#" -ne 1 ]; then
    echo "usage: $0 OUTPUT_DIR" >&2
    exit 2
fi

output=$1
script_dir=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
context=$script_dir/../tests/complex-rootfs
image=latc-complex-rootfs:debian12-20260824
temporary=$output.tmp.$$
container=

cleanup()
{
    if [ -n "$container" ]; then
        docker rm -f "$container" >/dev/null 2>&1 || true
    fi
    rm -rf "$temporary"
}
trap cleanup EXIT HUP INT TERM

docker build --platform linux/amd64 --pull=false -t "$image" "$context"
container=$(docker create --platform linux/amd64 "$image")
mkdir -p "$temporary/rootfs" "$temporary/metadata"
docker export "$container" | tar --no-same-owner \
  --exclude='./dev/*' --exclude='./proc/*' --exclude='./sys/*' \
  -xpf - -C "$temporary/rootfs"
mkdir -p "$temporary/rootfs/dev" "$temporary/rootfs/proc" \
  "$temporary/rootfs/sys"
# LAT's loader preflight resolves this link from the host namespace.  Keep it
# relative so it remains inside the exported rootfs rather than pointing at
# the LoongArch host's /lib.
ln -snf ../lib/x86_64-linux-gnu/ld-linux-x86-64.so.2 \
  "$temporary/rootfs/usr/lib64/ld-linux-x86-64.so.2"

docker run --rm --platform linux/amd64 "$image" \
  dpkg-query -W -f='${binary:Package}\t${Version}\n' | \
  LC_ALL=C sort >"$temporary/metadata/packages.tsv"
docker run --rm --platform linux/amd64 "$image" sh -ec \
  'sha256sum /usr/bin/python3 /usr/bin/git /usr/bin/sqlite3 /usr/bin/redis-server /usr/bin/redis-cli' \
  >"$temporary/metadata/binaries.sha256"
cp "$context/source.json" "$temporary/metadata/source.json"
(
    cd "$temporary/rootfs"
    find . -type l -printf 'LINK %p -> %l\n' | LC_ALL=C sort
    find . -type f -print0 | LC_ALL=C sort -z | xargs -0 sha256sum
) >"$temporary/metadata/rootfs-files.sha256"
sha256sum "$temporary/metadata/rootfs-files.sha256" | \
  cut -d ' ' -f 1 >"$temporary/metadata/rootfs-tree.sha256"

docker rm "$container" >/dev/null
container=
rm -rf "$output"
mv "$temporary" "$output"
trap - EXIT HUP INT TERM
printf 'complex rootfs ready: %s\n' "$output"
printf 'tree sha256: '
cat "$output/metadata/rootfs-tree.sha256"
