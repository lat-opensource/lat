# Eflags link regression

The AOT module linker and JIT linker share `lat_eflags_link_actions()`.
The translator supplies patch locations; the module linker does not classify
instructions by opcode. Native images retain the original flag instruction and
unlinked recovery stubs so incremental merges can reconsider successor liveness.

| Existing optimization | AOT treatment |
| --- | --- |
| TB-local flag reduction | Original translation/flag reduction pipeline |
| TU flag liveness and pattern lowering | Original TU pipeline; no second lowering pass |
| Ordinary TB flag instruction elimination | Shared link decision, exported patch location and original instruction |
| XCOMISX recovery stub bypass | Shared link decision, exported stub location, direct local target |
| XCOMISX with intervening instructions | Original lowering computes flags before the intervening instructions; no link-time recovery stub |
| OPT_BCC taken edge | Same exclusion as the JIT instruction patcher |
| IS_TU_JMP | Already lowered by the TU path; ordinary JIT linking also excludes it |
| Missing or unsupported local successor, cross-module target | Preserve flag calculation and recovery stub |

The native format is version 6 (48-byte TB records, including validated
conditional and indirect exit offsets). Runtime module ABI remains
version 2. Regenerate older native images; do not reuse them with this compiler.

`make -C tools/latc test` includes the shared-decision matrix and native format,
link and incremental-merge regression in `test-aot-v2-module-pack.c`.

For execution coverage, compile `x86-eflags-link.c` on an x86 host inside Docker:

```sh
gcc -O2 -fno-inline -static -o x86-eflags-link x86-eflags-link.c
./x86-eflags-link > expected.out
```

Transfer both outputs to the LoongArch host, then run:

```sh
python3 tools/latc/tests/test-aot-v2-eflags-link.py \
  PREFIX/bin/latc PREFIX/bin/latx-x86_64 PREFIX/lib ROOTFS \
  /absolute/path/x86-eflags-link /absolute/path/expected.out NEW_WORKDIR
```

The script checks actual NOP and recovery-stub patches in the fixture functions,
retention of live-flag sites, and exact output against the x86-native reference
for JIT, first module load and two strict warm runs. The first load uses a
precompiled cache; it is not an empty-cache compilation latency measurement.
It also requires conditional-exit metadata from the real translator for all
CMP/TEST widths with both live and dead successors. The recorded sites must
contain an in-TB conditional branch, and linking must preserve its opcode and
registers. `conditional-proof.json` records these producer checks.

Cases include CMP/TEST widths, SUB, BT, intervening MOV instructions, AND/JNE,
immediate SHR/JNE, and COMIS/UCOMIS single/double precision, with and without
intervening instructions. Other arithmetic and
shift cases also check the original translation pipeline. Do not infer that an
instruction has a link-time patch site merely because its output test passes.

Live successors read flags with PUSHFQ. Dead successors overwrite arithmetic
flags and DF before PUSHFQ; overwriting only arithmetic flags would still leave
DF live under the original translator's liveness rules. Undefined AF is omitted
from comparisons; shift counts are one so the observed OF is defined.

This regression complements installation, signal, fork, invalidation and dynamic
application tests. It does not establish all asynchronous signal interleavings
or add TU exit optimizations absent from the existing JIT/TU implementation.
