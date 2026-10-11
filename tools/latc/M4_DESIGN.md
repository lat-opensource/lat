# AOT v2 M4 design

## Processes

The LAT runner is only a client. It detects an eligible file-backed executable
ELF mapping and sends a nonblocking request to the current user's `latcd` Unix
socket. `latcd` owns source snapshots, compiler children, validation, cache
publication, duplicate suppression, and failure throttling.

The translated process never forks to compile. It also never waits for a
compile result while holding an mmap lock or executing guest code.

## Request protocol

The local transport is `AF_UNIX` with `SOCK_SEQPACKET`. The only protocol is
version 2. Every request has a fixed-size header. `SUBMIT_KEYS` carries source
and key-set descriptors, `FLUSH_SOURCE` carries one source descriptor, and
`FLUSH_ALL` carries no descriptor.

```c
struct LatcdRequestV2 {
    uint32_t magic;
    uint16_t version;
    uint16_t size;
    uint32_t operation;
    uint32_t priority;
    uint64_t request_id;
    uint64_t sequence;
};
```

The request contains no source path. The service rejects a truncated packet,
unknown flags, multiple descriptors, non-regular files, writable descriptors,
non-x86 ELF files, and files larger than the configured input limit.

`latcd --once` accepts one request and replies after publication. The resident
service acknowledges a valid queued request promptly; compilation completion
is observable through the cache and service counters, not by blocking the
runtime client.

## Stable identity

After receiving the descriptor, `latcd` verifies it with `fstat`, reads it into
a private temporary snapshot, and calculates SHA-256 over the copied bytes.
The service compares file metadata before and after copying. A size or identity
change rejects the request. Compilation always reads the snapshot, never the
caller's original descriptor.

The job key is:

```text
(source_sha256, aot_abi, codegen_id, required_host_features, page_size)
```

M4 initially retains the M3 lookup-compatible published path
`<cache>/<source_sha256>.so`. Variant metadata is validated inside the module;
the resident service later adds a small atomic `current` index while retaining
the flat path until the runner has migrated.

## Compilation and publication

For one job, the service performs these steps:

1. Create cache and temporary directories with mode `0700`.
2. Copy the source FD to a mode `0600` temporary snapshot and calculate SHA-256.
3. Start `latc compile-module` with trusted daemon configuration and the
   snapshot path. The request cannot choose the compiler or output path.
4. Apply CPU-time, address-space, output-size, and file-descriptor limits to the
   compiler child.
5. Inspect the produced module and compare its embedded source SHA-256 with the
   snapshot digest. Normal runtime validation still checks ABI, codegen ID,
   required ISA features, page size, tables, and relocations when loading it.
6. `fsync` the module, atomically rename it to its final cache path, and
   `fsync` the containing directory.
7. Remove all temporary files on success or failure.

An already valid final module makes the request a cache hit. Invalid existing
files are never trusted or overwritten in place; the service builds a new
temporary module and replaces the path atomically only after validation.

## Queue and failure handling

The resident service has one compiler worker by default. Startup objects use a
higher priority than libraries discovered later. The queue has fixed limits on
job count and total input bytes. Duplicate job keys attach to the existing job
instead of starting another compiler.

The initial defaults are 64 waiting jobs, 4 GiB of waiting snapshots, and one
worker. Compiler children are limited to 60 CPU seconds, 1 TiB of virtual
address space, 2 GiB output files, and 256 open descriptors. The address-space
limit must remain large because the LAT compiler runner reserves guest virtual
address ranges; an 8 GiB trial forced its PIE into the guest `0x400000` range
and was rejected by the loader.

A failed job records the job key, reason class, compiler identity, attempt
count, and retry time. The delay grows with repeated failures and is capped.
Changing source bytes or the compiler/codegen identity creates a different key
and does not inherit the old failure. Negative-cache records have an expiry and
a size bound.

## Runtime submission

The runner keeps a small set of source identities already submitted by the
process. It submits only a validated x86 `ET_EXEC` or `ET_DYN` regular file that
owns an executable `PT_LOAD`. Main image and interpreter submission happens
after their descriptors are available. DSO submission happens after the mmap
ELF tracker has identified the complete image.

Socket creation and sending are nonblocking. `ENOENT`, `ECONNREFUSED`, queue
full, and short-lived resource errors increment diagnostics and return to JIT.
No compile callback can replace a module in the current registry instance.

`LATX_AOT_V2_LATCD_SOCKET` enables submission. The runner creates a nonblocking
`SOCK_SEQPACKET`, connects, transfers the existing tracker FD with
`SCM_RIGHTS | MSG_DONTWAIT`, and closes the socket without reading a response.
The first two discovered ELF identities use startup priority 200; later DSOs
use library priority 100. A SHA-256 set prevents a reload at another load bias
from submitting the same bytes again.

Dynamic compilation requires the daemon's trusted `--x86-rootfs` setting. It
becomes `LAT_LD_PREFIX` only in the compiler child's minimal environment; the
request cannot choose or override it.

## Security boundary

`latcd` is per-user and creates its socket and cache below a user-owned `0700`
directory. It checks peer credentials, does not follow cache symlinks, does not
accept path or command arguments from requests, and drops unexpected ancillary
descriptors. Compiler children receive only the snapshot, output path, runner,
runtime directory, and a minimal environment.

## Implementation status

The implementation uses only the version 2 packet, `SCM_RIGHTS` transfer, stable source
snapshot and SHA-256 calculation, x86 ELF checks, existing-cache validation,
compiler invocation, output inspection, source-digest comparison, read-only
artifact mode, `fsync`, atomic rename, and temporary-file cleanup. The
`--submit` mode is a synchronous diagnostic client for this one-request step.

`WI-2274` adds `--serve`, same-UID peer checks, a one-second incomplete-request
timeout, one worker, priority and byte-bounded queues, SHA-256 duplicate
suppression, bounded exponential failure delays, compiler resource limits, a
minimal child environment, atomic JSON counters, atomic `current` indexes, and
SIGTERM cleanup for queued and active jobs.

`WI-2275` adds the nonblocking runner client, staging integration, per-process
SHA-256 suppression, startup/library priorities, and submission diagnostics.
It does not load a module produced during the same process.
