#!/bin/bash
# Run on the LoongArch build host.  The guest is tu-boundary.S built for the
# matching x86 ABI; no guest libraries or root privileges are needed.
set -euo pipefail
build=$(realpath "$1")
guest=$(realpath "$2")
case "$guest" in
    *32) binary="$build/latx-i386" ;;
    *64) binary="$build/latx-x86_64" ;;
    *) echo 'guest filename must end in 32 or 64' >&2; exit 2 ;;
esac
test_dir=$(dirname "$(realpath "$0")")
# Exercise the page budget and rollback without HRAS layout changes.
export LATX_HRAS=0
source "$test_dir/tu-test-common.sh"
result=$(mktemp -d "${TMPDIR:-/tmp}/latu-tu-boundary.XXXXXX")
export HOME="$result/home"
mkdir -p "$HOME"
export TU_LAYOUT="$result/layout.json"
echo "Results: $result"

# Use the selected translator's exact target configuration for TB offsets.
bits=64; [[ "$guest" == *32 ]] && bits=32
python3 - "$build" "$test_dir" "$result" "$bits" <<'PY'
import json
import shlex
import subprocess
import sys
from pathlib import Path

build, tests, result = map(Path, sys.argv[1:4])
entries = json.loads((build / 'compile_commands.json').read_text())
command = next(x['command'] for x in entries
               if x['file'].endswith('optimization/tu.c')
               and ('libqemu-i386-' in x['command']) ==
                   (str(sys.argv[4]) == '32'))
args = shlex.split(command)
subprocess.run(args[:args.index('-MD')] +
               [str(tests / 'tu-boundary-layout.c'),
                '-o', str(result / 'layout')],
               cwd=build, check=True)
PY
"$result/layout" > "$TU_LAYOUT"

for mode in 0 1 2 4; do
    LATX_AOT=0 "$binary" "$guest" "$mode"
done
LATX_AOT=3 "$binary" "$guest" 0
cache=$(cache_path "$HOME" 3)
wait_cache "$cache"
export LATX_AOT_GENERATE=0
sha256sum "$cache" > "$result/cache.sha256"

export TU_FAULT_PC=0x$(nm "$guest" | awk '$3 == "f0" {print $1}')
export TU_JMP_PC=0x$(nm "$guest" | awk '$3 == "f1_jump" {print $1}')
for fault in first second code tbs links fallback exhaust; do
    LATX_AOT=3 TU_FAULT=$fault gdb -nx -q -batch \
        -x "$test_dir/tu-recover-failure-gdb.py" -ex "quit 1" \
        --args "$binary" "$guest" 0 > "$result/failure-$fault.log" 2>&1
    tail -1 "$result/failure-$fault.log"
done

for mode in 0 1 2 4; do
    TU_GUEST_MODE=$mode LATX_AOT=3 gdb -nx -q -batch \
        -x "$test_dir/tu-boundary-gdb.py" \
        -ex "quit 1" \
        --args "$binary" "$guest" "$mode" > "$result/aot-$mode.log" 2>&1
    tail -1 "$result/aot-$mode.log"
    flock -w 30 "$cache.lock" true
    if [[ "$mode" == 0 ]]; then
        sha256sum "$cache" > "$result/cache.sha256"
    elif [[ "$mode" == 1 || "$mode" == 2 ]]; then
        sha256sum -c "$result/cache.sha256"
    fi
done
TU_GUEST_MODE=0 LATX_AOT=0 gdb -nx -q -batch -x "$test_dir/tu-boundary-gdb.py" \
        -ex "quit 1" \
    --args "$binary" "$guest" 0 > "$result/jit.log" 2>&1
tail -1 "$result/jit.log"
for aot in 1 2; do
  # Generate page-local code in its own cache; never consume the cross-page
  # cache left by mode 3.  Mode 2 shares the page policy with mode 1.
  LATX_AOT_GENERATE=1 LATX_AOT=$aot "$binary" "$guest" 0
  page_cache=$(cache_path "$HOME" "$aot")
  wait_cache "$page_cache"
  for mode in 0 1 2; do
    TU_GUEST_MODE=$mode LATX_AOT=$aot gdb -nx -q -batch \
        -x "$test_dir/tu-boundary-gdb.py" -ex "quit 1" \
        --args "$binary" "$guest" "$mode" \
        > "$result/page-$aot-mode-$mode.log" 2>&1
    tail -1 "$result/page-$aot-mode-$mode.log"
  done
done
LATX_AOT=3 "$binary" "$guest" 0
flock -w 30 "$cache.lock" true
# Mode 4 legitimately adds previously unexecuted entry paths to the cache.
sha256sum "$binary"
echo PASS
