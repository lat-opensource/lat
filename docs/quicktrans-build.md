# Building LATU with Quicktrans

Quicktrans is a Git submodule at `target/i386/latx/quicktrans`, hosted at
https://github.com/lat-opensource/lat-quicktrans. The `open-source` branch is
configured for explicit remote updates; reproducible builds use the exact
commit recorded by LATU, not the current branch tip.

From a checkout of this LATU integration branch:

```sh
git submodule sync --recursive
git submodule update --init --recursive
./latxbuild/build64.sh -c -f
```

Use a LoongArch build host with the normal LATU build dependencies installed.
The `-f` option compiles Quicktrans into `build64/latx-x86_64`. Do not use
`git submodule update --remote` when reproducing a pinned build.

Quicktrans is disabled at runtime by default. To select it:

```sh
LATX_FAST_TRANSLATOR=1 ./build64/latx-x86_64 /path/to/x86_64-program
```

Strict mode defaults to enabled: a Quicktrans translation failure is reported
instead of silently using the legacy translator. Set
`LATX_FAST_TRANSLATOR_STRICT=0` to permit legacy fallback, or
`LATX_FAST_TRANSLATOR=0` to select the legacy path. These settings are read
once, before the first translated block; changing them during execution is
not supported.

`LATX_QUICKTRANS_RSP_MERGE=0` disables deferred stack-pointer adjustment merging.
`LATX_INSTPTN` selects the existing instruction-pattern optimization mask.
Quicktrans handles ordinary 64-bit translation blocks; its own translation
path does not provide translation-unit (TU) compilation.

## Fault recovery contract

Quicktrans publishes a distinct block flag and instruction search records.
LATU must use those records rather than legacy IR1/stack recovery. The records
include deferred stack-pointer adjustments and floating-point recovery state.
Signal entry and return preserve both hard and soft x87 representations.

Quicktrans returns machine code followed by one encoded search table. The last
instruction end offset includes exit code and identifies the machine-code end;
LATU reuses the existing table at that offset instead of encoding another copy.
Any change that adds trailing code or literal data must preserve this contract.
