#!/bin/sh
set -eu

latx=$(readlink -f "$1")
reference=${QEMU_X86_64:-qemu-x86_64}
if [ -z "${LATX_NO_LBT_GUEST_DIR:-}" ] &&
   ! command -v "$reference" >/dev/null 2>&1; then
    echo "SKIP: need qemu-x86_64 or native x86 reference fixtures"
    exit 77
fi
test_dir=$(dirname "$(readlink -f "$0")")
workdir=$(mktemp -d)
trap 'rm -rf "$workdir"' EXIT HUP INT TERM

. "$(dirname "$(readlink -f "$0")")/no-lbt-test-lib.sh"

no_lbt_build_guest "$test_dir/latx-eflags-branch-differential.S" \
    "$workdir/eflags-branch"
no_lbt_build_guest "$test_dir/latx-eflags-differential.S" \
    "$workdir/eflags-random" -DTRACE_RECORD=1

if [ -n "${LATX_NO_LBT_GUEST_DIR:-}" ]; then
    cp "$LATX_NO_LBT_GUEST_DIR/branch.reference" "$workdir/branch.reference"
    cp "$LATX_NO_LBT_GUEST_DIR/random.reference" "$workdir/random.reference"
else
    "$reference" "$workdir/eflags-branch" >"$workdir/branch.reference"
    "$reference" "$workdir/eflags-random" >"$workdir/random.reference"
fi
"$latx" -latx-lbt 0 "$workdir/eflags-branch" >"$workdir/branch.latx"
cmp "$workdir/branch.reference" "$workdir/branch.latx"

"$latx" -latx-lbt 0 "$workdir/eflags-random" \
    >"$workdir/random.latx"
python3 "$test_dir/compare-latx-eflags-differential.py" \
    "$workdir/random.reference" "$workdir/random.latx"

echo "PASS: x86 reference and no-LBT LATX agree on EFLAGS consumers and branches"
