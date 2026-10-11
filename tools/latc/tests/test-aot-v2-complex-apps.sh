#!/bin/sh
set -eu

if [ "$#" -ne 6 ]; then
    echo "usage: $0 LATCD LATC RUNNER RUNTIME_DIR ROOTFS WORKDIR" >&2
    exit 2
fi

latcd=$1
latc=$2
runner=$3
runtime_dir=$4
rootfs=$5
work=$6
metadata_dir=$(dirname "$rootfs")/metadata
script_dir=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
phases=${LATC_COMPLEX_PHASES:-jit}
applications=${LATC_COMPLEX_APPLICATIONS:-python git sqlite redis}
timeout_seconds=${LATC_COMPLEX_TIMEOUT:-60}
stress_seconds=${LATC_COMPLEX_STRESS_SECONDS:-0}
stress_phases=${LATC_COMPLEX_STRESS_PHASES:-jit warm}
latcd_cpu_seconds=${LATC_COMPLEX_LATCD_CPU_SECONDS:-60}
latcd_workers=${LATC_COMPLEX_LATCD_WORKERS:-0}
compiler_wait_seconds=${LATC_COMPLEX_COMPILER_WAIT_SECONDS:-1200}
require_compiler_success=${LATC_COMPLEX_REQUIRE_COMPILER_SUCCESS:-0}
cache_source=${LATC_COMPLEX_CACHE_SOURCE:-}
warm_socket=${LATC_COMPLEX_WARM_SOCKET:-1}
require_registered=${LATC_COMPLEX_REQUIRE_REGISTERED:-1}
warm_report=${LATC_COMPLEX_WARM_REPORT:-0}
min_aot_percent=${LATC_COMPLEX_MIN_AOT_PERCENT:-0}
coverage_applications=${LATC_COMPLEX_COVERAGE_APPLICATIONS:-$applications}
require_no_fork_jit=${LATC_COMPLEX_REQUIRE_NO_FORK_JIT:-0}
flush_only=${LATC_COMPLEX_FLUSH_ONLY:-0}
max_flush_ms=${LATC_COMPLEX_MAX_FLUSH_MS:-0}
sqlite_clients=${LATC_COMPLEX_SQLITE_CLIENTS:-4}
min_git_aot_percent=${LATC_COMPLEX_MIN_GIT_AOT_PERCENT:-0}
if [ "$min_aot_percent" = 0 ] && [ "$min_git_aot_percent" != 0 ]; then
    min_aot_percent=$min_git_aot_percent
    coverage_applications=git
fi
max_submissions=${LATC_COMPLEX_MAX_SUBMISSIONS:-1024}
daemon_pid=
fault_daemon_pid=
redis_pid=
subscriber_pid=
socket=
cache=
stats=
phase_home=
export LATC_FAKE_REAL=$latc

cleanup()
{
    if [ -n "$fault_daemon_pid" ] && kill -0 "$fault_daemon_pid" 2>/dev/null; then
        kill -TERM "$fault_daemon_pid" 2>/dev/null || true
        wait "$fault_daemon_pid" 2>/dev/null || true
    fi
    if [ -n "$redis_pid" ] && kill -0 "$redis_pid" 2>/dev/null; then
        kill -TERM -"$redis_pid" 2>/dev/null || true
        wait "$redis_pid" 2>/dev/null || true
    fi
    if [ -n "$subscriber_pid" ] && kill -0 "$subscriber_pid" 2>/dev/null; then
        kill -TERM -"$subscriber_pid" 2>/dev/null || true
        wait "$subscriber_pid" 2>/dev/null || true
    fi
    compiler_groups=$(ps -eo pgid=,comm=,args= | \
      awk -v marker="$cache/.tmp/source-" \
          '($2 == "latc" || $2 == "sh") && index($0, marker) { print $1 }' | \
      sort -u)
    for compiler_group in $compiler_groups; do
        kill -TERM -"$compiler_group" 2>/dev/null || true
    done
    if [ -n "$compiler_groups" ]; then
        sleep 0.1
    fi
    for compiler_group in $compiler_groups; do
        kill -KILL -"$compiler_group" 2>/dev/null || true
    done
    if [ -n "$daemon_pid" ] && kill -0 "$daemon_pid" 2>/dev/null; then
        kill -TERM "$daemon_pid" 2>/dev/null || true
        wait "$daemon_pid" 2>/dev/null || true
    fi
}
trap cleanup EXIT HUP INT TERM

for binary in python3 git sqlite3 redis-server redis-cli; do
    test -x "$rootfs/usr/bin/$binary" || {
        echo "complex rootfs lacks /usr/bin/$binary" >&2
        exit 2
    }
done
test -f "$metadata_dir/source.json" || {
    echo "complex rootfs lacks sibling metadata/source.json" >&2
    exit 2
}

rm -rf "$work"
mkdir -m 700 -p "$work"
mkdir -m 700 "$work/cache"
mkdir -m 700 "$work/home"
cache=$work/cache
if [ -n "$cache_source" ]; then
    test -d "$cache_source"
    cp -a "$cache_source/." "$cache/"
fi
socket=$work/latcd.sock
stats=$work/latcd.json

run_guest()
{
    stderr=$1
    shift
    command_log=$(dirname "$stderr")/commands.jsonl
    status=0
    setsid env HOME="$phase_home" LC_ALL=C.UTF-8 \
      LD_LIBRARY_PATH="$runtime_dir" \
      LATC_DISABLE_PRETRANSLATE=1 LATC_COMPLEX_RUNNER="$runner" \
      LATC_COMPLEX_ROOTFS="$rootfs" $phase_environment \
      timeout -k 2s "$timeout_seconds" "$runner" -L "$rootfs" "$@" \
      2>"$stderr" || status=$?
    python3 "$script_dir/record-complex-result.py" \
      --exit-code "$status" --stderr "$stderr" \
      --environment "$phase_environment" "$command_log" -- \
      "$runner" -L "$rootfs" "$@"
    return "$status"
}

run_python()
{
    phase_dir=$1
    run_guest "$phase_dir/python.stderr" "$rootfs/usr/bin/python3" \
      "$script_dir/complex-python.py" >"$phase_dir/python.stdout"
    grep -q '^PYTHON_OK ' "$phase_dir/python.stdout"
}

run_git()
{
    phase_dir=$1
    repo=$phase_dir/git-repo
    mkdir "$repo"
    git_guest()
    {
        temporary_stderr=$phase_dir/git.stderr.tmp
        status=0
        run_guest "$temporary_stderr" "$rootfs/usr/bin/git" \
          -C "$repo" "$@" || status=$?
        cat "$temporary_stderr" >>"$phase_dir/git.stderr"
        rm -f "$temporary_stderr"
        return "$status"
    }
    : >"$phase_dir/git.stdout"
    : >"$phase_dir/git.stderr"
    git_guest init -q >>"$phase_dir/git.stdout"
    git_guest config user.name LATC >>"$phase_dir/git.stdout"
    git_guest config user.email latc@example.invalid >>"$phase_dir/git.stdout"
    printf 'base\n' >"$repo/base.txt"
    git_guest add base.txt >>"$phase_dir/git.stdout"
    git_guest commit -qm base >>"$phase_dir/git.stdout"
    git_guest checkout -qb feature >>"$phase_dir/git.stdout"
    printf 'feature\n' >"$repo/feature.txt"
    git_guest add feature.txt >>"$phase_dir/git.stdout"
    git_guest commit -qm feature >>"$phase_dir/git.stdout"
    git_guest checkout -q master >>"$phase_dir/git.stdout"
    printf 'master\n' >"$repo/master.txt"
    git_guest add master.txt >>"$phase_dir/git.stdout"
    git_guest commit -qm master >>"$phase_dir/git.stdout"
    git_guest merge -q --no-edit feature >>"$phase_dir/git.stdout"
    before=$(git_guest rev-parse 'HEAD^{tree}' | tail -1)
    git_guest diff --exit-code >>"$phase_dir/git.stdout"
    git_guest repack -ad >>"$phase_dir/git.stdout"
    git_guest gc >>"$phase_dir/git.stdout"
    git_guest fsck --strict >>"$phase_dir/git.stdout"
    after=$(git_guest rev-parse 'HEAD^{tree}' | tail -1)
    test -n "$before" && test "$before" = "$after"
    printf 'GIT_OK %s\n' "$after" >>"$phase_dir/git.stdout"
}

run_sqlite()
{
    phase_dir=$1
    database=$phase_dir/test.db
    printf '%s\n' 'PRAGMA journal_mode=WAL;' \
      'CREATE TABLE values_table(id INTEGER PRIMARY KEY, value INTEGER NOT NULL);' \
      >"$phase_dir/sqlite-create.sql"
    run_guest "$phase_dir/sqlite.stderr" "$rootfs/usr/bin/sqlite3" \
      -batch "$database" <"$phase_dir/sqlite-create.sql" \
      >"$phase_dir/sqlite.stdout"
    grep -q '^wal$' "$phase_dir/sqlite.stdout"

    sqlite_pids=
    client=0
    test "$sqlite_clients" -gt 0
    test $((1000 % sqlite_clients)) -eq 0
    rows_per_client=$((1000 / sqlite_clients))
    while [ "$client" -lt "$sqlite_clients" ]; do
        begin=$((client * rows_per_client + 1))
        end=$((begin + rows_per_client - 1))
        {
            echo '.timeout 30000'
            echo 'BEGIN IMMEDIATE;'
            printf 'WITH RECURSIVE n(x) AS (VALUES(%s) UNION ALL SELECT x+1 FROM n WHERE x<%s) INSERT INTO values_table SELECT x,x FROM n;\n' \
              "$begin" "$end"
            echo 'COMMIT;'
        } >"$phase_dir/sqlite-client-$client.sql"
        run_guest "$phase_dir/sqlite-client-$client.stderr" \
          "$rootfs/usr/bin/sqlite3" -batch "$database" \
          <"$phase_dir/sqlite-client-$client.sql" \
          >"$phase_dir/sqlite-client-$client.stdout" &
        sqlite_pids="$sqlite_pids $!"
        client=$((client + 1))
    done
    for sqlite_pid in $sqlite_pids; do
        wait "$sqlite_pid"
    done
    printf '%s\n' 'SELECT count(*),sum(value) FROM values_table;' \
      'PRAGMA integrity_check;' >"$phase_dir/sqlite-check.sql"
    run_guest "$phase_dir/sqlite-check.stderr" "$rootfs/usr/bin/sqlite3" \
      -batch "$database" <"$phase_dir/sqlite-check.sql" \
      >>"$phase_dir/sqlite.stdout"
    grep -q '^1000|500500$' "$phase_dir/sqlite.stdout"
    grep -q '^ok$' "$phase_dir/sqlite.stdout"
    echo SQLITE_OK >>"$phase_dir/sqlite.stdout"
}

run_redis()
{
    phase_dir=$1
    redis_port=${LATC_COMPLEX_REDIS_PORT:-$(python3 -c \
      'import socket; s=socket.socket(); s.bind(("127.0.0.1",0)); print(s.getsockname()[1]); s.close()')}
    mkdir "$phase_dir/redis-data"
    start_redis()
    {
        suffix=$1
        setsid env HOME="$phase_home" LC_ALL=C.UTF-8 \
          LD_LIBRARY_PATH="$runtime_dir" LATC_DISABLE_PRETRANSLATE=1 \
          LATC_COMPLEX_RUNNER="$runner" LATC_COMPLEX_ROOTFS="$rootfs" \
          $phase_environment "$runner" -L "$rootfs" \
          "$rootfs/usr/bin/redis-server" --bind 127.0.0.1 \
          --protected-mode no --port "$redis_port" --save '' --appendonly no \
          --daemonize no --dir "$phase_dir/redis-data" \
          >"$phase_dir/redis-$suffix.stdout" \
          2>"$phase_dir/redis-$suffix.stderr" &
        redis_pid=$!
    }
    wait_redis()
    {
        n=0
        while :; do
            if run_guest "$phase_dir/redis-cli.stderr" \
              "$rootfs/usr/bin/redis-cli" -h 127.0.0.1 -p "$redis_port" ping \
              >"$phase_dir/redis-cli.stdout" && \
              grep -q '^PONG$' "$phase_dir/redis-cli.stdout"; then
                return
            fi
            kill -0 "$redis_pid" 2>/dev/null || return 1
            n=$((n + 1))
            [ "$n" -lt 100 ] || return 1
            sleep 0.05
        done
    }
    start_redis first
    wait_redis

    redis_client_pids=
    client=0
    while [ "$client" -lt 4 ]; do
        printf 'MULTI\nSET client:%s value:%s\nINCR counter\nEXEC\n' \
          "$client" "$client" >"$phase_dir/redis-client-$client.commands"
        run_guest "$phase_dir/redis-client-$client.stderr" \
          "$rootfs/usr/bin/redis-cli" -h 127.0.0.1 -p "$redis_port" --raw \
          <"$phase_dir/redis-client-$client.commands" \
          >"$phase_dir/redis-client-$client.stdout" &
        redis_client_pids="$redis_client_pids $!"
        client=$((client + 1))
    done
    for redis_client_pid in $redis_client_pids; do
        wait "$redis_client_pid"
    done
    test "$(run_guest "$phase_dir/redis-counter.stderr" \
      "$rootfs/usr/bin/redis-cli" -h 127.0.0.1 -p "$redis_port" \
      get counter)" = 4

    setsid env HOME="$phase_home" LC_ALL=C.UTF-8 \
      LD_LIBRARY_PATH="$runtime_dir" LATC_DISABLE_PRETRANSLATE=1 \
      LATC_COMPLEX_RUNNER="$runner" LATC_COMPLEX_ROOTFS="$rootfs" \
      $phase_environment timeout -k 1s 5s "$runner" -L "$rootfs" \
      "$rootfs/usr/bin/redis-cli" -h 127.0.0.1 -p "$redis_port" --raw \
      SUBSCRIBE latc-channel >"$phase_dir/redis-subscribe.stdout" \
      2>"$phase_dir/redis-subscribe.stderr" &
    subscriber_pid=$!
    n=0
    while ! grep -qx '1' "$phase_dir/redis-subscribe.stdout"; do
        kill -0 "$subscriber_pid" 2>/dev/null || return 1
        n=$((n + 1))
        [ "$n" -lt 250 ] || return 1
        sleep 0.02
    done
    test "$(run_guest "$phase_dir/redis-publish.stderr" \
      "$rootfs/usr/bin/redis-cli" -h 127.0.0.1 -p "$redis_port" \
      PUBLISH latc-channel message)" = 1
    n=0
    while ! grep -q '^message$' "$phase_dir/redis-subscribe.stdout"; do
        n=$((n + 1))
        [ "$n" -lt 100 ] || return 1
        sleep 0.02
    done
    kill -TERM -"$subscriber_pid" 2>/dev/null || true
    wait "$subscriber_pid" 2>/dev/null || true
    subscriber_pid=

    run_guest "$phase_dir/redis-bgsave.stderr" \
      "$rootfs/usr/bin/redis-cli" -h 127.0.0.1 -p "$redis_port" \
      BGSAVE >"$phase_dir/redis-bgsave.stdout"
    n=0
    while :; do
        persistence=$(run_guest "$phase_dir/redis-info.stderr" \
          "$rootfs/usr/bin/redis-cli" -h 127.0.0.1 -p "$redis_port" \
          --raw INFO persistence | tr -d '\r')
        if printf '%s\n' "$persistence" | grep -q '^rdb_bgsave_in_progress:0' && \
          printf '%s\n' "$persistence" | grep -q '^rdb_last_bgsave_status:ok'; then
            break
        fi
        n=$((n + 1))
        [ "$n" -lt 500 ] || return 1
        sleep 0.05
    done
    run_guest "$phase_dir/redis-stop.stderr" "$rootfs/usr/bin/redis-cli" \
      -h 127.0.0.1 -p "$redis_port" shutdown nosave >/dev/null
    wait "$redis_pid"
    redis_pid=

    start_redis restart
    wait_redis
    test "$(run_guest "$phase_dir/redis-restart-counter.stderr" \
      "$rootfs/usr/bin/redis-cli" -h 127.0.0.1 -p "$redis_port" \
      get counter)" = 4
    client=0
    while [ "$client" -lt 4 ]; do
        test "$(run_guest "$phase_dir/redis-restart-$client.stderr" \
          "$rootfs/usr/bin/redis-cli" -h 127.0.0.1 -p "$redis_port" \
          get "client:$client")" = "value:$client"
        client=$((client + 1))
    done
    run_guest "$phase_dir/redis-restart-stop.stderr" \
      "$rootfs/usr/bin/redis-cli" -h 127.0.0.1 -p "$redis_port" \
      shutdown nosave >/dev/null
    wait "$redis_pid"
    redis_pid=
    echo REDIS_OK >"$phase_dir/redis-result.stdout"
}

wait_for_compiler()
{
    n=0
    while :; do
        if [ -f "$stats" ] && python3 - "$stats" 2>/dev/null <<'PY'
import json
import sys
s = json.load(open(sys.argv[1]))
assert s["active_jobs"] == 0 and s["queue_depth"] == 0
PY
        then
            if [ "$require_compiler_success" -eq 1 ] &&
               ! python3 - "$stats" 2>/dev/null <<'PY'
import json
import sys
assert json.load(open(sys.argv[1]))["failed"] == 0
PY
            then
                echo "latcd reported a compiler failure" >&2
                cat "$stats" >&2
                return 1
            fi
            return
        fi
        n=$((n + 1))
        [ "$n" -lt $((compiler_wait_seconds * 10)) ] || return 1
        sleep 0.1
    done
}

stop_fault_daemon()
{
    if [ -n "$fault_daemon_pid" ] && kill -0 "$fault_daemon_pid" 2>/dev/null; then
        kill -TERM "$fault_daemon_pid"
        wait "$fault_daemon_pid"
    fi
    fault_daemon_pid=
}

start_fault_daemon()
{
    label=$1
    compiler=$2
    fault_cache=$3
    fault_socket=$work/$label.sock
    fault_stats=$work/$label-stats.json
    rm -f "$fault_socket" "$fault_stats"
    "$latcd" --serve --socket "$fault_socket" --cache-dir "$fault_cache" \
      --latc "$compiler" --runner "$runner" --runtime-dir "$runtime_dir" \
      --x86-rootfs "$rootfs" --stats "$fault_stats" --workers 1 \
      >"$work/$label-latcd.stdout" 2>"$work/$label-latcd.stderr" &
    fault_daemon_pid=$!
    n=0
    while [ ! -S "$fault_socket" ] && \
          kill -0 "$fault_daemon_pid" 2>/dev/null; do
        n=$((n + 1))
        [ "$n" -lt 500 ] || return 1
        sleep 0.01
    done
}

run_fault_python()
{
    label=$1
    fault_environment=$2
    fault_dir=$work/faults/$label
    mkdir "$fault_dir"
    phase_home=$fault_dir/home
    mkdir -m 700 "$phase_home"
    phase_environment=$fault_environment
    printf '%s\n' "$phase_environment" >"$fault_dir/environment.txt"
    run_python "$fault_dir"
    if [ -f "$work/faults/gold/python.stdout" ]; then
        cmp "$work/faults/gold/python.stdout" "$fault_dir/python.stdout"
    fi
}

run_faults()
{
    test -n "$(find "$cache" -maxdepth 1 -type f -name '*.so' -print -quit)"
    mkdir "$work/faults"

    run_fault_python gold 'LATX_AOT=0 LATX_AOT_V2_REPORT=1'

    missing_cache=$work/faults/missing-cache
    mkdir -m 700 "$missing_cache"
    run_fault_python missing-latcd \
      "LATX_AOT=0 LATX_AOT_V2_CACHE_DIR=$missing_cache LATX_AOT_V2_LATCD_SOCKET=$work/no-latcd.sock LATX_AOT_V2_REPORT=1 LATX_AOT_V2_MAX_SUBMISSIONS=1"

    current_cache=$work/faults/corrupt-current-cache
    mkdir -m 700 "$current_cache"
    cp -a "$cache"/. "$current_cache"/
    for current in "$current_cache"/*.current; do
        chmod u+w "$current"
        printf '{invalid\n' >"$current"
    done
    run_fault_python corrupt-current \
      "LATX_AOT=0 LATX_AOT_V2_CACHE_DIR=$current_cache LATX_AOT_V2_LATCD_SOCKET=$work/no-latcd.sock LATX_AOT_V2_REPORT=1 LATX_AOT_V2_MAX_SUBMISSIONS=1"

    module_cache=$work/faults/corrupt-module-cache
    mkdir -m 700 "$module_cache"
    cp -a "$cache"/. "$module_cache"/
    for module in "$module_cache"/*.so; do
        chmod u+w "$module"
        dd if=/dev/zero of="$module" bs=64 count=1 conv=notrunc \
          >/dev/null 2>&1
    done
    run_fault_python corrupt-module \
      "LATX_AOT=0 LATX_AOT_V2_CACHE_DIR=$module_cache LATX_AOT_V2_LATCD_SOCKET=$work/no-latcd.sock LATX_AOT_V2_REPORT=1 LATX_AOT_V2_MAX_SUBMISSIONS=1"

    compiler_cache=$work/faults/compiler-failure-cache
    mkdir -m 700 "$compiler_cache"
    start_fault_daemon compiler-failure "$script_dir/fake-latc-fail.sh" \
        "$compiler_cache"
    run_fault_python compiler-failure \
      "LATX_AOT=0 LATX_AOT_V2_CACHE_DIR=$compiler_cache LATX_AOT_V2_LATCD_SOCKET=$fault_socket LATX_AOT_V2_REPORT=1 LATX_AOT_V2_MAX_SUBMISSIONS=1"
    n=0
    while ! python3 - "$fault_stats" 2>/dev/null <<'PY'
import json
import sys
s = json.load(open(sys.argv[1]))
assert s["failed"] > 0
assert s["active_jobs"] == 0 and s["queue_depth"] == 0
PY
    do
        n=$((n + 1))
        [ "$n" -lt 300 ] || return 1
        sleep 0.1
    done
    stop_fault_daemon

    readonly_cache=/proc/latc-aot-v2-readonly-$$
    start_fault_daemon readonly-cache "$latc" "$readonly_cache" || true
    run_fault_python readonly-cache \
      "LATX_AOT=0 LATX_AOT_V2_CACHE_DIR=$readonly_cache LATX_AOT_V2_LATCD_SOCKET=$work/readonly-cache.sock LATX_AOT_V2_REPORT=1 LATX_AOT_V2_MAX_SUBMISSIONS=1"
    stop_fault_daemon

    printf 'complex fault fallbacks: PASS cases=5\n'
    python3 - "$work/faults/result.json" <<'PY'
import json
import sys

json.dump({"cases": 5, "exit_code": 0, "phase": "faults"},
          open(sys.argv[1], "w"), sort_keys=True)
PY
}

run_phase()
{
    phase=$1
    if [ "$phase" = faults ]; then
        run_faults
        return
    fi
    case "$phase" in
      jit) phase_storage=$work/phase-jit000 ;;
      cold) phase_storage=$work/phase-cold00 ;;
      warm) phase_storage=$work/phase-warm00 ;;
      strict) phase_storage=$work/phase-strict ;;
    esac
    mkdir "$phase_storage"
    ln -s "$(basename "$phase_storage")" "$work/$phase"
    phase_root=$phase_storage
    phase_home=$work/home
    case "$phase" in
      jit)
        phase_environment='LATX_AOT=0'
        ;;
      cold|warm|strict)
        if [ "$phase" = cold ]; then
            phase_environment="LATX_AOT=0 LATX_AOT_V2_CACHE_DIR= LATX_AOT_V2_MODULE= LATX_AOT_V2_LATCD_SOCKET=$socket LATX_AOT_V2_REPORT=1 LATX_AOT_V2_MAX_SUBMISSIONS=$max_submissions"
        elif [ "$phase" = strict ]; then
            phase_environment="LATX_AOT=0 LATX_AOT_V2_CACHE_DIR=$cache LATX_AOT_V2_LATCD_SOCKET=$socket LATX_AOT_V2_REPORT=1 LATX_AOT_V2_MAX_SUBMISSIONS=$max_submissions LATX_AOT_V2_STRICT=1 LATC_STRICT_FILE_AOT=1"
        elif [ "$warm_socket" -eq 1 ]; then
            phase_environment="LATX_AOT=0 LATX_AOT_V2_CACHE_DIR=$cache LATX_AOT_V2_LATCD_SOCKET=$socket LATX_AOT_V2_REPORT=1 LATX_AOT_V2_MAX_SUBMISSIONS=$max_submissions"
        else
            phase_environment="LATX_AOT=0 LATX_AOT_V2_CACHE_DIR=$cache"
            if [ "$warm_report" -eq 1 ]; then
                phase_environment="$phase_environment LATX_AOT_V2_REPORT=1"
            fi
        fi
        ;;
      *) echo "unknown complex application phase: $phase" >&2; exit 2 ;;
    esac
    printf '%s\n' "$phase_environment" >"$phase_root/environment.txt"
    phase_started=$(date +%s)
    iteration=1
    while :; do
        if [ "$iteration" -eq 1 ]; then
            iteration_dir=$phase_root
        else
            iteration_dir=$phase_root/iteration-$iteration
            mkdir "$iteration_dir"
        fi
        : >"$iteration_dir/app-timings.tsv"
        for application in $applications; do
            application_started=$(python3 "$script_dir/monotonic-ns.py")
            "run_$application" "$iteration_dir"
            application_finished=$(python3 "$script_dir/monotonic-ns.py")
            printf '%s\t%s\n' "$application" \
              "$((application_finished - application_started))" \
              >>"$iteration_dir/app-timings.tsv"
        done
        case " $stress_phases " in
          *" $phase "*) ;;
          *) break ;;
        esac
        [ "$stress_seconds" -gt 0 ] || break
        now=$(date +%s)
        [ $((now - phase_started)) -lt "$stress_seconds" ] || break
        iteration=$((iteration + 1))
    done
    phase_finished=$(date +%s)
    printf 'iterations=%s elapsed_seconds=%s\n' "$iteration" \
      "$((phase_finished - phase_started))" >"$phase_root/stress-result.txt"
    if { [ "$phase" = warm ] || [ "$phase" = strict ]; } &&
       [ "$require_registered" -eq 1 ]; then
        grep -Rqs 'module=registered' "$phase_root"/*.stderr || {
            echo "warm phase registered no AOT module" >&2
            return 1
        }
    fi
    check_coverage=0
    if [ "$phase" = strict ]; then
        check_coverage=1
    elif [ "$phase" = warm ]; then
        case " $phases " in *' strict '*) ;; *) check_coverage=1;; esac
    fi
    if [ "$check_coverage" -eq 1 ] && [ "$min_aot_percent" != 0 ]; then
        no_fork_option=
        if [ "$require_no_fork_jit" -eq 1 ]; then
            no_fork_option=--require-no-fork-jit
        fi
        python3 "$script_dir/check-aot-v2-coverage.py" \
          $no_fork_option "$phase_root" "$min_aot_percent" \
          $coverage_applications
    fi
    python3 - "$phase_root/result.json" "$phase" "$iteration" \
      "$((phase_finished - phase_started))" "$phase_root/app-timings.tsv" \
      "$applications" <<'PY'
import json
import sys

timings = {}
for line in open(sys.argv[5]):
    application, elapsed = line.rstrip().split("\t")
    timings[application] = int(elapsed)
json.dump({
    "applications": sys.argv[6].split(),
    "application_elapsed_ns": timings,
    "application_total_ns": sum(timings.values()),
    "elapsed_seconds": int(sys.argv[4]),
    "exit_code": 0,
    "iterations": int(sys.argv[3]),
    "phase": sys.argv[2],
}, open(sys.argv[1], "w"), sort_keys=True)
PY
    printf 'complex phase %s: PASS iterations=%s elapsed=%ss\n' \
      "$phase" "$iteration" "$((phase_finished - phase_started))"
}

case " $phases $warm_socket " in
  *' cold '*|*' warm 1 '*|*' strict '*)
    flush_option=
    if [ "$flush_only" -eq 1 ]; then
        flush_option=--flush-only
    fi
    "$latcd" --serve --socket "$socket" --cache-dir "$cache" \
      --latc "$latc" --runner "$runner" --runtime-dir "$runtime_dir" \
      --x86-rootfs "$rootfs" --stats "$stats" \
      --cpu-seconds "$latcd_cpu_seconds" --workers "$latcd_workers" \
      $flush_option \
      >"$work/latcd.stdout" 2>"$work/latcd.stderr" &
    daemon_pid=$!
    n=0
    while [ ! -S "$socket" ] && kill -0 "$daemon_pid" 2>/dev/null; do
        n=$((n + 1))
        [ "$n" -lt 500 ] || exit 1
        sleep 0.01
    done
    test -S "$socket"
    ;;
esac

for phase in $phases; do
    run_phase "$phase"
    if [ "$phase" = cold ] ||
       { [ "$phase" = warm ] && [ "$warm_socket" -eq 1 ]; }; then
        flush_started=$(python3 "$script_dir/monotonic-ns.py")
        "$latcd" --flush-all --socket "$socket" \
          >"$work/$phase/flush-all.txt"
        flush_finished=$(python3 "$script_dir/monotonic-ns.py")
        flush_elapsed_ms=$(((flush_finished - flush_started) / 1000000))
        printf 'flush_all_ms=%s\n' "$flush_elapsed_ms" \
          >>"$work/$phase/flush-all.txt"
        grep -q '^status=0$' "$work/$phase/flush-all.txt"
        if [ "$max_flush_ms" -gt 0 ] &&
           [ "$flush_elapsed_ms" -gt "$max_flush_ms" ]; then
            echo "AOT flush ${flush_elapsed_ms}ms exceeds ${max_flush_ms}ms" >&2
            exit 1
        fi
        cp "$stats" "$work/$phase/latcd-after-flush.json"
        if [ "$require_compiler_success" -eq 1 ]; then
            python3 - "$stats" <<'PY'
import json
import sys
stats = json.load(open(sys.argv[1]))
assert stats["failed"] == 0, stats
assert stats["active_jobs"] == 0 and stats["queue_depth"] == 0, stats
PY
        fi
    fi
done

if [ -n "$daemon_pid" ]; then
    kill -TERM "$daemon_pid"
    wait "$daemon_pid"
    daemon_pid=
fi

printf 'test-aot-v2-complex-apps: PASS phases=%s\n' "$phases"
