# AOT v2 M5 test plan

## Artifact parser

- Accept the generated valid synthetic module through both FD and memory APIs.
- Test every truncation length from zero through the full fixture size.
- Mutate ELF, program-header, section-header, note, dynamic, string, symbol,
  and relocation fields at zero, maximum, wraparound, and off-by-one values.
- Run a fixed-seed mutation corpus under ASan and UBSan with a per-case size
  bound. A rejected input is normal; crash, sanitizer report, or hang fails.
- Build the optional libFuzzer target with Clang and record a bounded run on
  x86 and `3a6000-25g` when the toolchain is present.
- Turn every sanitizer or parser correctness finding into a named deterministic
  case before fixing it.

## Executable mapping and SMC

- Replace an AOT-backed page with `MAP_FIXED` and prove later dispatch does not
  enter the old translation.
- Cover partial and complete `munmap`, execute removal and restoration through
  `mprotect`, and private file mappings modified after becoming writable.
- Modify code from the current thread and another thread, including a write
  crossing a guest page boundary.
- Repeatedly map the original ELF at new load biases and prove only a newly
  validated registration can restore AOT dispatch.
- Verify anonymous executable memory stays on JIT and cannot borrow an AOT
  target with the same guest address.

## Signals and module lifetime

- Cause synchronous faults at the start, middle, and end of translated blocks
  and assert the delivered x86 PC and register state.
- Cover helper-generated faults, nested signals, alternate signal stacks,
  signal masking, and `sigreturn` resume.
- Deliver signals while another thread unloads or invalidates a module and
  verify no stale PC-map or module pointer is used.
- Run 100 `dlopen`/call/`dlclose` cycles and reload at different addresses.

## Production regression and measurements

- Run all architecture-independent tests locally and on `3a6000-25g`.
- Run M1 static glibc hello, M2 dynamic hello and ELF semantics, M3 DSO reload,
  and M4 daemon cold/warm/concurrent/failure tests.
- Regenerate and run all 12 SPECint2000 train modules. Limit every benchmark to
  60 seconds and do not run ref.
- Record startup time, RSS, shared executable pages, module registration time,
  AOT coverage, JIT fallback, runtime TB generation, and steady runtime.
- Compare AOT v2, old AOT, LATC M4, and native LoongArch with identical inputs,
  host settings, and cache state. Report per-test translation efficiency and
  its geometric mean.

## WI-2278 result

On 2026-08-26, the x86 development host passed `make test-aot-v2`,
`make test-aot-v2-sanitize`, and `make test-aot-v2-libfuzzer`. The fixed-seed
test checked every truncation length, named alignment and integer-boundary
cases, and 100,000 deterministic mutations. ASan and UBSan reported no error.
The valid-ELF-seeded libFuzzer run completed 100,000 inputs with no crash or
sanitizer report and reached 496 code edges, including dynamic symbols and
relocations.

After a clean rebuild on `3a6000-25g`, the synthetic parser tests, registry
tests, real LoongArch shared-module load, and bad-descriptor rejection passed.
The deterministic mutation test completed 100,000 inputs normally and again
under ASan with no report. The first real-module run found that a valid relative
relocation may contain a one-past segment address; the validator retains that
ELF behaviour while still requiring relocation writes to target writable,
non-executable storage.

The Loongnix host has no Clang, and its GCC installation lacks `libubsan`, so
libFuzzer and UBSan were not available there. This is a host toolchain limit;
the identical parser source passed both sanitizers on x86 and ASan on
LoongArch.
