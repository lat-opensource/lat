#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-only
set -euo pipefail

if [[ $# -lt 3 || $# -gt 4 ]]; then
    echo "usage: $0 <latx-hbr-off> <latx-hbr-on> <guest> [output-dir]" >&2
    exit 2
fi

off=$1
on=$2
guest=$3
out=${4:-./hbr-diff-results}
mkdir -p "$out"

if "$off" "$guest" >"$out/off.stdout" 2>"$out/off.stderr"; then
    off_status=0
else
    off_status=$?
fi
if "$on" "$guest" >"$out/on.stdout" 2>"$out/on.stderr"; then
    on_status=0
else
    on_status=$?
fi

printf '%s\n' "$off_status" >"$out/off.status"
printf '%s\n' "$on_status" >"$out/on.status"
diff -u "$out/off.stdout" "$out/on.stdout"
diff -u "$out/off.stderr" "$out/on.stderr"
diff -u "$out/off.status" "$out/on.status"
