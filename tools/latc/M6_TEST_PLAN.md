# AOT v2 M6 test plan

## Signals and diagnostics

- First prove the old `concurrent_invalidation` fixture has no second thread and cannot overlap
  `dlclose` with signal recovery.
- Add a barrier-controlled two-thread test for signal recovery during `dlclose`, `munmap`,
  `mprotect`, and guest code-write invalidation. Run each interleaving repeatedly.
- Assert boundary, middle, end, and helper faults restore guest RIP, all supported GPRs, RFLAGS,
  signal mask, vector state, altstack, nested delivery, and sigreturn state.
- Assert diagnostic lookup returns module digest, load instance generation, and exact guest PC;
  invalid or ambiguous PCs return an explicit failure.

## Lifetime and concurrency

- Count live and retired modules, instances, snapshots, and stats before and after at least 100000
  lifecycle operations. After a reader grace period, retained counts must be bounded.
- Stress dispatch, signal lookup, mmap notification, unload, and statistics reporting concurrently.
- Run ThreadSanitizer on architecture-independent registry/index tests when supported, plus ASan and
  UBSan. Convert every finding into a deterministic regression test.

## Mapping and platform behaviour

- Verify failed mmap/mprotect/mremap does not unnecessarily lose a valid AOT registration.
- Verify RW to RX and moved file mappings use AOT only after source digest validation and a new
  generation; changed bytes and anonymous mappings remain on JIT.
- On LASX hardware accept LASX and LSX fixtures; with mocked HWCAP reject LASX on LSX-only systems.
- Test cache quotas, eviction during concurrent submissions, worker limits, crash recovery, and
  rejection or immutable copying of owner-writable external modules.

## Dynamic workload and performance

- Select at least one reproducible dynamically linked application with multiple DSOs and plugin
  reload. Record cold/warm startup, module registration, AOT hits, JIT fallback, runtime TBs, and
  RSS/PSS throughout a long run.
- Run two simultaneous processes and use smaps/PSS to prove executable-page sharing.
- Compare AOT v2, old AOT, M4, and native LoongArch using identical train inputs, fixed CPU and
  frequency policy. Run at least five samples per benchmark and report medians and geometric means.
- Run all existing M1-M5 tests and SPECint2000 train 12/12 with the existing 60-second limit.
