#!/bin/sh
set -eu

script=$1
script_dir=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
work=$(mktemp -d "${TMPDIR:-/tmp}/latc-one-pass-test.XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM

printf guest >"$work/guest"
python3 "$script_dir/tb_key_set.py" "$work/guest" \
    "$work/input.tbset" 0x1000:0x3

cat >"$work/stage1" <<'EOF'
#!/bin/sh
printf 'run\n' >>"$FAKE_RUN_COUNT"
printf '%s\n' "${LATC_NATIVE_STATIC_CHECK-unset}" >>"$FAKE_CHECK_COUNT"
{
    printf '0x401000 0x1\n'
    printf '0x402000 0x3\n'
} >"$LATC_NATIVE_MISSING_OUT"
exit 1
EOF
chmod +x "$work/stage1"

cat >"$work/latc" <<'EOF'
#!/bin/sh
if [ "$1" = emit-static-tbset ]; then
    cp "$FAKE_TBSET" "$4"
    exit 0
fi
if [ "$1" = compile ]; then
    shift
    while [ "$#" -gt 0 ]; do
        if [ "$1" = -o ]; then
            cp "$FAKE_STAGE1" "$2"
            chmod +x "$2"
            exit 0
        fi
        shift
    done
fi
exit 2
EOF
chmod +x "$work/latc"

set +e
FAKE_STAGE1="$work/stage1" FAKE_RUN_COUNT="$work/run-count" \
    FAKE_CHECK_COUNT="$work/check-count" LATC_NATIVE_STATIC_CHECK=1 \
    "$script" "$work/latc" "$work/runner" "$work/guest" \
    "$work/output" "$work/input.tbset" >"$work/stdout" 2>"$work/stderr"
status=$?
set -e

test "$status" -ne 0
test "$(wc -l <"$work/run-count")" -eq 1
grep -q '0x401000 0x1' "$work/stderr"
grep -q '0x402000 0x3' "$work/stderr"
! grep -q 'supplement round' "$work/stderr"
test "$(cat "$work/check-count")" = unset

set +e
FAKE_STAGE1="$work/stage1" FAKE_RUN_COUNT="$work/run-count-static" \
    FAKE_CHECK_COUNT="$work/check-count-static" FAKE_TBSET="$work/input.tbset" \
    "$script" "$work/latc" "$work/runner" "$work/guest" \
    "$work/output-static" >"$work/static.stdout" 2>"$work/static.stderr"
status=$?
set -e
test "$status" -ne 0
test "$(cat "$work/check-count-static")" = 1
test "$(wc -l <"$work/run-count-static")" -eq 1
set +e
FAKE_STAGE1="$work/stage1" FAKE_RUN_COUNT="$work/run-count-explicit" \
    FAKE_CHECK_COUNT="$work/check-count-explicit" \
    "$script" --static-tbset --module "$work/latc" "$work/runner" \
    "$work/guest" "$work/output-explicit" "$work/input.tbset" \
    >"$work/explicit.stdout" 2>"$work/explicit.stderr"
status=$?
set -e
test "$status" -ne 0
test "$(cat "$work/check-count-explicit")" = 1
test "$(wc -l <"$work/run-count-explicit")" -eq 1
printf 'test-compile-native-one-pass: PASS\n'
