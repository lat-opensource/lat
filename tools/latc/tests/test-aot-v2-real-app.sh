#!/bin/sh
set -eu

if [ "$#" -ne 7 ]; then
    echo "usage: $0 LATCD LATC RUNNER RUNTIME_DIR ROOTFS X86_APP WORKDIR" >&2
    exit 2
fi

latcd=$1
latc=$2
runner=$3
runtime_dir=$4
rootfs=$5
app=$6
work=$7
rm -rf "$work"
mkdir -m 700 -p "$work"
mkdir -m 700 "$work/cache"
socket=$work/latcd.sock
stats=$work/latcd.json
daemon_pid=

cleanup()
{
    if [ -n "$daemon_pid" ] && kill -0 "$daemon_pid" 2>/dev/null; then
        kill -TERM "$daemon_pid" 2>/dev/null || true
        wait "$daemon_pid" 2>/dev/null || true
    fi
}
trap cleanup EXIT HUP INT TERM

"$latcd" --serve --socket "$socket" --cache-dir "$work/cache" \
  --latc "$latc" --runner "$runner" --runtime-dir "$runtime_dir" \
  --x86-rootfs "$rootfs" --stats "$stats" --workers 2 \
  >"$work/latcd.stdout" 2>"$work/latcd.stderr" &
daemon_pid=$!
n=0
while [ ! -S "$socket" ] && kill -0 "$daemon_pid" 2>/dev/null; do
    n=$((n + 1))
    [ "$n" -lt 500 ] || exit 1
    sleep 0.01
done
test -S "$socket"

workload='rounds=$1; sum=0; i=0; while ((i < rounds)); do ((sum += i % 97)); ((i++)); done; printf "%s\n" "$sum"'
expected()
{
    python3 -c 'n=int(__import__("sys").argv[1]); print(sum(i % 97 for i in range(n)))' "$1"
}

run_foreground()
{
    rounds=$1
    name=$2
    start=$(date +%s%N)
    env LD_LIBRARY_PATH="$runtime_dir" LATX_AOT=0 \
      LATX_AOT_V2_CACHE_DIR="$work/cache" \
      LATX_AOT_V2_LATCD_SOCKET="$socket" LATX_AOT_V2_REPORT=1 \
      LATC_DISABLE_PRETRANSLATE=1 LATC_STATS_OUT="$work/$name.stats.json" \
      timeout -k 2s 180s "$runner" -L "$rootfs" "$app" \
      -c "$workload" latc-real-app "$rounds" \
      >"$work/$name.stdout" 2>"$work/$name.stderr"
    end=$(date +%s%N)
    echo $(((end - start) / 1000000)) >"$work/$name.ms"
    test "$(cat "$work/$name.stdout")" = "$(expected "$rounds")"
}

startup_rounds=${LATC_REAL_APP_ROUNDS:-250000}
startup_samples=${LATC_REAL_APP_STARTUP_SAMPLES:-5}
case "$startup_samples" in
    ''|*[!0-9]*) echo "invalid LATC_REAL_APP_STARTUP_SAMPLES" >&2; exit 2 ;;
esac
[ "$startup_samples" -ge 1 ] && [ "$startup_samples" -le 20 ] || {
    echo "LATC_REAL_APP_STARTUP_SAMPLES must be between 1 and 20" >&2
    exit 2
}
run_foreground "$startup_rounds" cold
n=0
while :; do
    if [ -f "$stats" ] && python3 - "$stats" 2>/dev/null <<'PY'
import json, sys
s = json.load(open(sys.argv[1]))
assert s["compiled"] >= 3 and s["active_jobs"] == 0
PY
    then
        break
    fi
    n=$((n + 1))
    [ "$n" -lt 3600 ] || exit 1
    sleep 0.05
done

run_foreground "$startup_rounds" warm
sample_index=2
while [ "$sample_index" -le "$startup_samples" ]; do
    run_foreground "$startup_rounds" "warm.$sample_index"
    sample_index=$((sample_index + 1))
done
grep -Eq 'module=registered aot_lookups=[1-9][0-9]*' "$work/warm.stderr"
grep -Eq 'registration_ns=[1-9][0-9]*' "$work/warm.stderr"

if [ "${LATC_REAL_APP_STARTUP_ONLY:-0}" = 1 ]; then
    kill -TERM "$daemon_pid"
    wait "$daemon_pid"
    daemon_pid=
    python3 - "$work" >"$work/startup-report.json" <<'PY'
import json, pathlib, re, statistics, sys
work = pathlib.Path(sys.argv[1])
pattern = re.compile(r"module stats source=([0-9a-f]{64}).*module=(\w+) "
                     r"aot_lookups=(\d+) jit_fallbacks=(\d+) "
                     r"registration_ns=(\d+)")
modules = []
for match in pattern.finditer((work / "warm.stderr").read_text()):
    modules.append({"source": match.group(1), "state": match.group(2),
                    "aot_lookups": int(match.group(3)),
                    "jit_fallbacks": int(match.group(4)),
                    "registration_ns": int(match.group(5))})
runtime = json.load(open(work / "warm.stats.json"))
warm_paths = [work / "warm.ms"] + sorted(work.glob("warm.*.ms"))
warm_samples = [int(path.read_text()) for path in warm_paths]
report = {
    "cold_ms": int((work / "cold.ms").read_text()),
    "warm_ms": int(statistics.median(warm_samples)),
    "warm_samples_ms": warm_samples,
    "modules": modules,
    "runtime_tb_gen_attempts": runtime.get("runtime_tb_gen_attempts"),
    "runtime_tb_gen_calls": runtime.get("runtime_tb_gen_calls"),
}
assert any(m["state"] == "registered" and m["aot_lookups"] > 0
           for m in modules), modules
json.dump(report, sys.stdout, indent=2)
print()
PY
    echo "test-aot-v2-real-app-startup: PASS report=$work/startup-report.json"
    exit 0
fi

long_rounds=500000
env LD_LIBRARY_PATH="$runtime_dir" LATX_AOT=0 \
  LATX_AOT_V2_CACHE_DIR="$work/cache" LATX_AOT_V2_REPORT=1 \
  LATC_DISABLE_PRETRANSLATE=1 LATC_STATS_OUT="$work/long.stats.json" \
  "$runner" -L "$rootfs" "$app" -c "$workload" \
  latc-real-app "$long_rounds" >"$work/long.stdout" \
  2>"$work/long.stderr" &
long_pid=$!
: >"$work/resources.tsv"
sample=0
while kill -0 "$long_pid" 2>/dev/null; do
    if [ -r "/proc/$long_pid/smaps_rollup" ]; then
        if python3 - "$long_pid" "$sample" >>"$work/resources.tsv" <<'PY'
import pathlib, sys, time
pid, sample = sys.argv[1:]
values = {}
try:
    lines = pathlib.Path(f"/proc/{pid}/smaps_rollup").read_text().splitlines()
except FileNotFoundError:
    # The process can exit after the shell's readability check.
    sys.exit(1)
for line in lines:
    fields = line.split()
    if len(fields) >= 2 and fields[0].rstrip(":") in ("Rss", "Pss"):
        values[fields[0].rstrip(":")] = int(fields[1])
print(sample, time.monotonic_ns(), values.get("Rss", 0),
      values.get("Pss", 0), sep="\t")
PY
        then
            sample=$((sample + 1))
        fi
    fi
    sleep 0.1
done
wait "$long_pid"
test "$(cat "$work/long.stdout")" = "$(expected "$long_rounds")"
test "$(wc -l <"$work/resources.tsv")" -ge 3

dual_rounds=700000
dual_pids=
for id in 1 2; do
    env LD_LIBRARY_PATH="$runtime_dir" LATX_AOT=0 \
      LATX_AOT_V2_CACHE_DIR="$work/cache" LATX_AOT_V2_REPORT=1 \
      LATC_DISABLE_PRETRANSLATE=1 \
      "$runner" -L "$rootfs" "$app" -c "$workload" \
      latc-real-app "$dual_rounds" >"$work/dual.$id.stdout" \
      2>"$work/dual.$id.stderr" &
    dual_pids="$dual_pids $!"
done
set -- $dual_pids
first_pid=$1
second_pid=$2
n=0
while { [ ! -r "/proc/$first_pid/smaps" ] || \
        [ ! -r "/proc/$second_pid/smaps" ]; }; do
    n=$((n + 1))
    [ "$n" -lt 100 ] || exit 1
    sleep 0.05
done
sleep 0.5
python3 - "$first_pid" "$second_pid" "$work/cache" \
  >"$work/shared.json" <<'PY'
import json, pathlib, re, sys

cache = str(pathlib.Path(sys.argv[3]).resolve()) + "/"
def cache_maps(pid):
    result = {}
    current = None
    for line in pathlib.Path(f"/proc/{pid}/smaps").read_text().splitlines():
        if re.match(r"^[0-9a-f]+-[0-9a-f]+ ", line):
            fields = line.split(None, 5)
            current = fields[5] if len(fields) == 6 and fields[5].startswith(cache) else None
            if current:
                item = result.setdefault(current, {"rss_kb": 0, "pss_kb": 0,
                                                   "shared_clean_kb": 0})
        elif current:
            key, _, value = line.partition(":")
            if key == "Rss": result[current]["rss_kb"] += int(value.split()[0])
            elif key == "Pss": result[current]["pss_kb"] += int(value.split()[0])
            elif key == "Shared_Clean":
                result[current]["shared_clean_kb"] += int(value.split()[0])
    return result

processes = [cache_maps(sys.argv[1]), cache_maps(sys.argv[2])]
common = sorted(set(processes[0]) & set(processes[1]))
summary = {
    "pids": [int(sys.argv[1]), int(sys.argv[2])],
    "common_modules": common,
    "processes": processes,
    "combined_rss_kb": sum(v["rss_kb"] for p in processes for v in p.values()),
    "combined_pss_kb": sum(v["pss_kb"] for p in processes for v in p.values()),
    "combined_shared_clean_kb": sum(v["shared_clean_kb"] for p in processes for v in p.values()),
}
assert len(common) >= 3, summary
assert summary["combined_shared_clean_kb"] > 0, summary
assert summary["combined_pss_kb"] < summary["combined_rss_kb"], summary
json.dump(summary, sys.stdout, indent=2)
print()
PY
for pid in $dual_pids; do wait "$pid"; done
for id in 1 2; do
    test "$(cat "$work/dual.$id.stdout")" = "$(expected "$dual_rounds")"
done

kill -TERM "$daemon_pid"
wait "$daemon_pid"
daemon_pid=

python3 - "$work" >"$work/report.json" <<'PY'
import json, pathlib, re, statistics, sys
work = pathlib.Path(sys.argv[1])
pattern = re.compile(r"module stats source=([0-9a-f]{64}).*module=(\w+) "
                     r"aot_lookups=(\d+) jit_fallbacks=(\d+) "
                     r"registration_ns=(\d+)")
modules = []
for match in pattern.finditer((work / "warm.stderr").read_text()):
    modules.append({"source": match.group(1), "state": match.group(2),
                    "aot_lookups": int(match.group(3)),
                    "jit_fallbacks": int(match.group(4)),
                    "registration_ns": int(match.group(5))})
samples = []
for line in (work / "resources.tsv").read_text().splitlines():
    sample, timestamp, rss, pss = map(int, line.split("\t"))
    samples.append({"sample": sample, "monotonic_ns": timestamp,
                    "rss_kb": rss, "pss_kb": pss})
runtime = json.load(open(work / "warm.stats.json"))
shared = json.load(open(work / "shared.json"))
warm_paths = [work / "warm.ms"] + sorted(work.glob("warm.*.ms"))
warm_samples = [int(path.read_text()) for path in warm_paths]
registered = [m for m in modules if m["state"] == "registered"]
assert len(registered) >= 3, modules
assert all(m["aot_lookups"] > 0 for m in registered), modules
report = {
    "application": "/bin/bash",
    "cold_ms": int((work / "cold.ms").read_text()),
    "warm_ms": int(statistics.median(warm_samples)),
    "warm_samples_ms": warm_samples,
    "modules": modules,
    "runtime_tb_gen_attempts": runtime.get("runtime_tb_gen_attempts"),
    "runtime_tb_gen_calls": runtime.get("runtime_tb_gen_calls"),
    "resource_samples": samples,
    "rss_kb_min": min(s["rss_kb"] for s in samples),
    "rss_kb_max": max(s["rss_kb"] for s in samples),
    "pss_kb_min": min(s["pss_kb"] for s in samples),
    "pss_kb_max": max(s["pss_kb"] for s in samples),
    "shared_execution": shared,
}
json.dump(report, sys.stdout, indent=2)
print()
PY

echo "test-aot-v2-real-app: PASS report=$work/report.json"
