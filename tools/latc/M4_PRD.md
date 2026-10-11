# AOT v2 M4: on-demand compiler service

## Goal

Run x86 programs immediately through LAT JIT on an AOT cache miss while a
separate per-user `latcd` process builds a reusable AOT v2 module. A later
process or later module load can consume the published module.

## Required behaviour

- The runtime submits an already-open, read-only ELF file descriptor. It does
  not ask `latcd` to reopen a caller-provided path.
- `latcd` copies a stable source snapshot and computes its SHA-256 identity.
- Compilation runs outside the translated process and outside mmap locks.
- A module becomes visible only after compilation and full module validation
  succeed. Publication uses an atomic rename in the cache filesystem.
- Duplicate requests for the same source and codegen variant share one job.
- Failed jobs enter a bounded negative cache so repeated launches do not start
  an immediate compile loop.
- The runtime submits the main executable, interpreter, startup libraries, and
  later file-backed executable ELF mappings. Anonymous executable mappings are
  never submitted.
- Submission failure, daemon absence, and compiler failure never change the
  running process's JIT behaviour.

## Out of scope

- Consuming a newly built module in the process that requested it.
- System-wide or privileged daemon operation.
- Remote compilation, cache synchronization, or signed artifact distribution.
- Host AOT module reclamation after `dlclose`.

## Milestones

1. A one-request `latcd --once` receives one FD, builds, validates, and
   atomically publishes one module.
2. A resident service adds SHA-256 deduplication, a bounded priority queue,
   compiler limits, and a negative cache.
3. The AOT v2 runner submits eligible ELF mappings without waiting for a
   compile result.
4. Cold, warm, failure, and concurrent-process tests pass on 3A6000.

## Exit criteria

- A cold dynamic program finishes through JIT and leaves valid cache modules.
- Its next run loads those modules and reports AOT hits.
- Concurrent requests for identical bytes execute one compile and publish one
  valid artifact.
- Killing a compile, corrupt compiler output, a full queue, or an absent daemon
  leaves no visible partial module and does not stop the guest process.
