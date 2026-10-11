# AOT v2 Candidate Validation

This is engineering validation, not full application support or an official
SPEC result. Keep source/build identities, commands, exit codes and logs for
each candidate. Do not certify a new candidate with previous build results.

## Prerequisites

Use a LoongArch Linux host with GLib, Meson, Ninja and the project toolchain.
Provide an x86-64 glibc rootfs and matching x86 test fixtures. Several historical
Make recipes use CC for x86 assembly; do not let a host LoongArch compiler
silently replace those fixtures. Build them on x86 or with a verified x86
cross-toolchain, preserving the repository source and recipe options.

Set these absolute paths before the commands below:

```sh
SOURCE=/path/to/candidate
BUILD=/path/to/default-build
PREFIX=/path/to/default-install
ROOTFS=/path/to/x86-rootfs
WORK=/path/to/private-validation
FIXTURES=$SOURCE/tools/latc/build/tests
export TMPDIR="$WORK/tmp"
mkdir -p "$TMPDIR"
```

Use separate build, install and cache directories per configuration. Do not
run independent builds concurrently against the same source tree: some LAT
generation writes into the source tree. Record kernel, compiler versions,
configure options, Git HEAD and the uncommitted diff/untracked inputs.

## Build And Core Gates

Default O1 configuration used by this candidate:

```sh
mkdir -p "$BUILD"
cd "$BUILD"
"$SOURCE/configure" --target-list=x86_64-linux-user,i386-linux-user \
  --enable-latx --optimize-O1 --enable-tests --disable-docs --disable-tools \
  --disable-plugins --disable-debug-info --disable-werror \
  --without-default-features --disable-gnutls --disable-nettle \
  --disable-gcrypt --disable-curl --disable-bzip2 --disable-lzo \
  --disable-snappy --disable-libssh --disable-slirp --disable-capstone \
  --prefix="$PREFIX"
ninja -C "$BUILD" -j4 latx-x86_64 latx-i386 latc latcd
meson test -C "$BUILD" --suite lat-pr-fast --print-errorlogs
ninja -C "$BUILD" install
cd "$SOURCE"
make -C tools/latc test test-aot-v2-source-sync check-import
```

For AVX, select x86_64-linux-user only and add --enable-latx-avx-opt, with
another BUILD/PREFIX. Build and run the complete fast suite for both profiles.
Compiler, daemon, runner and runtime must report the same identity within
each installation; AVX and default profiles must not share their caches.

```sh
export LD_LIBRARY_PATH="$PREFIX/lib"
"$PREFIX/bin/latc" build-id
"$PREFIX/bin/latcd" --build-id
"$PREFIX/bin/latx-x86_64" --latc-build-id
sh tools/latc/tests/test-aot-v2-install.sh "$PREFIX" "$ROOTFS" \
  "$ROOTFS/usr/bin/true" "$WORK/install"
make -C tools/latc test-latcd-service \
  AOT_V2_CONFIG="$BUILD/config-host.mak" \
  AOT_V2_RUNNER="$PREFIX/bin/latx-x86_64" \
  AOT_V2_RUNTIME_DIR="$PREFIX/lib" X86_GUEST="$FIXTURES/x86-exit42"
sh tools/latc/tests/test-aot-v2-autostart.sh "$PREFIX" \
  "$FIXTURES/x86-static-hello" "$WORK/autostart"
```

Service includes controlled compiler-failure flush and static/runtime source
isolation tests. It builds and explicitly passes x86-exit-call42 for covered
key tests. Every TB key uses CODE64|PARALLEL flags3. Do not recover flags1
fixtures by weakening protocol validation.

Also validate a fresh DESTDIR installation, not just an incremental prefix.
Compiler helpers use the configured absolute prefix: DESTDIR is staging, not
a relocatable live install. Keep matching live helper files when testing the
staged tools, or configure a new prefix and install the complete set there.

## Affected Runtime Regressions

Run these with the matching AVX installation and ROOTFS:

```sh
export LAT_LD_PREFIX="$ROOTFS"
sh tools/latc/tests/test-aot-v2-fork.sh "$PREFIX/bin/latc" \
  "$PREFIX/bin/latx-x86_64" "$PREFIX/lib" "$ROOTFS" \
  "$FIXTURES/x86-aot-v2-fork" "$WORK/fork"
sh tools/latc/tests/test-aot-v2-invalidation.sh "$PREFIX/bin/latc" \
  "$PREFIX/bin/latx-x86_64" "$PREFIX/lib" "$ROOTFS" \
  "$FIXTURES/x86-aot-v2-invalidation" \
  "$FIXTURES/x86-aot-v2-invalidation-plugin.so" "$WORK/invalidation" 60
sh tools/latc/tests/test-aot-v2-xsave.sh "$PREFIX/bin/latc" \
  "$PREFIX/bin/latx-x86_64" "$PREFIX/lib" \
  "$FIXTURES/x86-xsave-roundtrip" "$WORK/xsave"
python3 tools/latc/tests/make-symbol-tbset.py \
  "$FIXTURES/x86-signal-recover" 'signal_.*_(site|fault|resume)' \
  "$WORK/signal.tbset"
sh tools/latc/scripts/compile-native-image.sh --module "$PREFIX/bin/latc" \
  "$PREFIX/bin/latx-x86_64" "$FIXTURES/x86-signal-recover" \
  "$WORK/signal.native" "$WORK/signal.tbset"
sh tools/latc/tests/test-aot-v2-signal-runner.sh "$PREFIX/bin/latc" \
  "$PREFIX/bin/latx-x86_64" "$PREFIX/lib" \
  "$FIXTURES/x86-signal-recover" "$WORK/signal.native" "$WORK/signal"
```

Compile x86-xsave-roundtrip.S with mask7; this covers plain XSAVE/XSAVEOPT/
XRSTOR, not unimplemented XSAVE64 variants. Module signal export intentionally
preserves unresolved direct targets; strict execution must still have zero
runtime TB generation. Standalone export is a separate acceptance category.

## Scope And Deferred Acceptance

Automatic cache setup is application-independent and does not enable static
precompile, collect-only or strict mode. No Wine/PE or i386 AOT v2 acceptance
is claimed. Static precompile rejects unsupported K/ZMM operands; diagnostic
failure is not complete static coverage. Current full-static dynamic/dlopen/
semantics tests can stop in the rootfs loader before runtime semantics.
Complex application incremental coverage has an unresolved libcrypto
missing-key result. Successful JIT/cold output is not strict AOT acceptance.
Keep failures explicit; do not delete requested keys or disable the guard.

Broad performance baseline, XSAVE64, int80/call-next, old-world and package
deployment acceptance are not included. Contribution provenance and historical
author DCO attestations are separate prerequisites, not runtime tests. Do not
fabricate permission or another contributor's sign-off.

## Pre-Submission SPEC Check

Resolve non-deferred review findings first. Then test SPEC2000 ref int using
the final matching installation and the user's chosen SPEC tree. Use a private
writable copy and empty isolated cache. Do not change system binfmt or original
results. SPEC speed submit configs require use_submit_for_speed=1:

```text
default=default=default=default:
use_submit_for_speed = 1
submit = /path/to/candidate-submit.sh $command
```

The wrapper explicitly invokes the matching runner/runtime in a clean
environment, LATX_AOT=0 LATX_AOT_V2=1, private HOME/XDG paths and rootfs.
Do not rely on binfmt. Leave collect-only/precompile unset for automatic v2
validation. Prove translator identity separately for nested guest exec: a
successful shell does not prove system binfmt selected this build.

Hold a host lock and run one SPEC tree at a time. Separate cache generation
from warm validation, archive each round before the next:

```sh
runspec -c candidate.cfg -i ref -n 1 int -I
```

Require 12 unique CINT Success records, valid raw results, zero official
comparison child return codes, empty comparison stderr and no .mis files.
Drain accepted jobs with matching latcd --flush-all, not a fixed sleep; retain
compile failures. Verify module identity/registration and report any warm
fallback/submission. Record wall/user/sys/RSS separately from SPEC raw
ratio/reported_time, reportable=0. This is not official submission or a speedup
comparison. Interrupted attempts are invalid and must remain separate.
