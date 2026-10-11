#!/bin/sh
set -eu

workdir=$1
rm -rf "$workdir"
mkdir -p "$workdir"

cat >"$workdir/input.profile" <<'EOF'
0x401020 1
0x401000 2
0x401020 4
EOF
cat >"$workdir/one.json" <<'EOF'
{"cfg_tbs":5,"continuation_tbs":2,"edge_target_tbs":1,"runtime_tb_gen_calls":2,"runtime_tb_gen_attempts":3,"runtime_program_tb_gen_calls":1,"runtime_system_tb_gen_calls":1,"runtime_program_tb_gen_attempts":1,"runtime_system_tb_gen_attempts":2,"runtime_first_pc":4198400,"runtime_first_cflags":16384,"aot_cache_hit":true,"bundle_verify_ns":10,"guest_extract_ns":20,"aot_prepare_ns":30}
EOF
cat >"$workdir/two.json" <<'EOF'
{"cfg_tbs":5,"runtime_tb_gen_calls":0,"runtime_program_tb_gen_calls":0,"runtime_system_tb_gen_calls":0,"runtime_first_pc":0}
EOF

PYTHONPATH="$(dirname "$0")/../spec2000" python3 - "$workdir" <<'PY'
import pathlib
import sys

from specint import (aggregate_stats, geometric_mean, merge_profile, run_spec,
                     sample_summary, selected_programs,
                     strict_aot_v2_env_lines)

work = pathlib.Path(sys.argv[1])
assert merge_profile(work / "input.profile", work / "merged.profile") == 2
assert work.joinpath("merged.profile").read_text() == \
    "0x401000 2\n0x401020 5\n"
stats = aggregate_stats([work / "one.json", work / "two.json"])
assert stats["processes"] == 2, stats
assert stats["cfg_tbs"] == 10, stats
assert stats["continuation_tbs"] == 2, stats
assert stats["edge_target_tbs"] == 1, stats
assert stats["runtime_tb_gen_calls"] == 2, stats
assert stats["runtime_tb_gen_attempts"] == 3, stats
assert stats["runtime_system_tb_gen_attempts"] == 2, stats
assert stats["runtime_first_pcs"] == [4198400], stats
assert stats["runtime_first_cflags"] == [16384], stats
assert stats["runtime_program_first_pcs"] == [4198400], stats
assert stats["aot_cache_hits"] == 1 and stats["aot_cache_misses"] == 1, stats
assert stats["aot_prepare_ns"] == 30, stats
summary = sample_summary([1.0, 2.0, 3.0])
assert summary["median"] == 2.0, summary
assert abs(geometric_mean([2.0, 8.0]) - 4.0) < 1e-12
assert selected_programs(["164.gzip"])[0][1] == "gzip_base.Of.gcc830.dyn"
env_lines = strict_aot_v2_env_lines(
    "/runtime dir", "/module dir/input.so", "/guest dir/input")
assert env_lines == [
    "export LD_LIBRARY_PATH='/runtime dir'${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}",
    "export LATX_AOT_V2_MODULE='/module dir/input.so'",
    "export LATX_AOT_V2_SOURCE='/guest dir/input'",
    "export LATX_AOT_V2_STRICT=1",
    "export LATC_DISABLE_PRETRANSLATE=1",
    "export LATC_STRICT_AOT=1",
], env_lines
spec = (work / "timeout-spec").resolve()
(spec / "result").mkdir(parents=True)
runner = spec / "myrun1.sh"
child_pid = (work / "timeout-child.pid").resolve()
runner.write_text(
    "#!/bin/sh\n"
    "%s -c \"import os,time; os.setpgrp(); "
    "open(%r, 'w').write(str(os.getpid())); time.sleep(30)\" &\n"
    "wait\n" % (sys.executable, str(child_pid)))
runner.chmod(0o755)
try:
    run_spec(spec, "train", "164.gzip", {}, work / "timeout.log",
             timeout=0.2)
except RuntimeError as error:
    assert "timed out" in str(error), error
else:
    raise AssertionError("run_spec timeout was not enforced")
pid = int(child_pid.read_text())
try:
    state = pathlib.Path("/proc/%d/stat" % pid).read_text().split()[2]
except FileNotFoundError:
    pass
else:
    if state != "Z":
        raise AssertionError("run_spec left child process %d running" % pid)
print("test-spec-tools: PASS")
PY

python3 "$(dirname "$0")/../spec2000/prepare-specint-train.py" --help >/dev/null
python3 "$(dirname "$0")/../spec2000/bench-specint-train.py" --help >/dev/null
python3 "$(dirname "$0")/../spec2000/run-specint-native.py" --help >/dev/null
python3 "$(dirname "$0")/../spec2000/compare-native-train.py" --help >/dev/null
python3 "$(dirname "$0")/../spec2000/compare-aot-v2-focused.py" --help >/dev/null
