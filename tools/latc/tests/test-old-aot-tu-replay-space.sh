#!/bin/sh
set -eu

repo_root=$(CDPATH= cd -- "$(dirname -- "$0")/../../.." && pwd)
replay_script="$repo_root/tools/latc/spec2000/test-old-aot-tu-replay.sh"
tmp_parent=${TMPDIR:-$HOME/tmp}
mkdir -p "$tmp_parent"
tmp_root=$(mktemp -d "$tmp_parent/latc-replay-space.XXXXXX")
trap 'rm -rf "$tmp_root"' EXIT HUP INT TERM

fixture="$tmp_root/fixture"
run_dir="$fixture/run dir"
expected_dir="$fixture/expected dir"
workdir="$fixture/work dir"
runner="$fixture/runner.sh"
guest="$fixture/guest"

mkdir -p "$run_dir" "$expected_dir"
printf '#!/bin/sh\n' >"$runner"
printf 'mkdir -p "$HOME/.cache/latx"\n' >>"$runner"
printf 'printf x >"$HOME/.cache/latx/cache file.aot2"\n' >>"$runner"
printf 'printf "A\\n"\n' >>"$runner"
chmod 755 "$runner"
printf '#!/bin/sh\nexit 0\n' >"$guest"
chmod 755 "$guest"

for input in input.source input.log input.graphic; do
    printf 'A\n' >"$expected_dir/$input.out"
done

sh "$replay_script" "$runner" "$guest" "$run_dir" "$expected_dir" \
    "$workdir" 10 >"$fixture/replay.log" 2>&1
cat "$fixture/replay.log"
test -s "$workdir/old-aot-tu-replay/input.source.cache.manifest.sha256"
test -s "$workdir/old-aot-tu-replay/input.log.cache.manifest.sha256"
test -s "$workdir/old-aot-tu-replay/input.graphic.cache.manifest.sha256"

echo "test-old-aot-tu-replay-space: PASS"
