#!/bin/sh
set -eu

emulator=$1
source_file=$2
workdir=$(mktemp -d)
trap 'rm -rf "$workdir"' EXIT HUP INT TERM

guest_dir=${LATX_SIGNAL_RETURN_GUEST_DIR:-$workdir}
if [ -n "${LATX_SIGNAL_RETURN_GUEST_DIR:-}" ]; then
    : # Guests must be built from the matching signal-return-bridge.S.
elif command -v clang-19 >/dev/null 2>&1; then
    clang=clang-19
elif command -v clang >/dev/null 2>&1; then
    clang=clang
else
    echo "SKIP: clang is required to build the x86_64 guest"
    exit 77
fi

for case_id in probe 0 1 2; do
    guest="$guest_dir/signal-return-bridge-$case_id"
    if [ -z "${LATX_SIGNAL_RETURN_GUEST_DIR:-}" ]; then
        if [ "$case_id" = probe ]; then
            define=-DPROBE_JRRA=1
        else
            define=-DRETURN_STYLE=$case_id
        fi
        "$clang" --target=x86_64-linux-gnu -fuse-ld=lld -nostdlib -static \
            -Wl,--build-id=none "$define" "$source_file" -o "$guest"
    fi
    if [ ! -x "$guest" ]; then
        echo "FAIL: guest executable missing: $guest" >&2
        exit 1
    fi
done

run_guest() {
    timeout 10s env LATX_AOT=0 LATX_KZT=0 "$emulator" \
        "$guest_dir/signal-return-bridge-$1"
}

probe_status=0
run_guest probe || probe_status=$?
case $probe_status in
0) echo "JRRA stack: observed a host return address from CALL" ;;
77) echo "JRRA stack disabled: running signal-return controls" ;;
*) echo "FAIL: JRRA stack probe status $probe_status" >&2
   exit "$probe_status" ;;
esac

for case_id in 0 1 2; do
    ret=0
    run_guest "$case_id" || ret=$?
    case $ret in
    0) echo "PASS: signal-return style=$case_id, nested signals and custom restorers" ;;
    10) echo "FAIL: rt_sigaction setup" >&2; exit "$ret" ;;
    11) echo "FAIL: wrong signal" >&2; exit "$ret" ;;
    12) echo "FAIL: nested return/order or sigreturn returned" >&2; exit "$ret" ;;
    13) echo "FAIL: handler/call/restorer counts" >&2; exit "$ret" ;;
    124) echo "FAIL: signal return timed out" >&2; exit "$ret" ;;
    *) echo "FAIL: style=$case_id unexpected status $ret" >&2; exit "$ret" ;;
    esac
done

if [ "$probe_status" = 77 ]; then
    echo "SKIP: bridge regression requires a JRRA-stack-enabled build (O2/O3 with LSFPU)"
    exit 77
fi
