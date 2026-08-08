LATU guest runtime diagnostics
==============================

Synopsis
--------

| **latu-runtime-manager status** [**--root** *HOST_ROOT*]
| **latu-runtime-manager current** [**--abi** *ABI*]
  [**--program** *PROGRAM*]
| **latu-runtime-manager list** [**--program** *PROGRAM*]
| **latu-runtime-manager inspect-root** [**--abi** *ABI*] [**--**] *ROOT*
| **latu-runtime-manager doctor** [**--abi** *ABI*]
  [**--program** *PROGRAM*]

Description
-----------

``latu-runtime-manager`` reports whether the LATX translators and their
selected x86 guest runtimes are usable.  It is a read-only diagnostic tool: it
does not download a runtime, change LATX configuration, register ``binfmt``,
or start a guest program.

The supported guest ABI names are ``x86_64`` and ``i386``.  The default
commands report both ABIs, so users do not need to choose one for routine
diagnosis.  ``--abi`` is an optional filter for scripts and targeted
troubleshooting.  The manager still tracks the ABIs independently because one
translator may be absent and existing LATX configurations may select different
runtime roots.  A single root containing both loader trees remains the simplest
layout when both translators are used.

Except for ``status``, command output is newline-delimited JSON with
``schema_version`` set to 1.  Paths are JSON strings, so spaces, control
characters, and non-ASCII characters cannot change the record boundaries.

Commands
--------

``status``
  Report whether ``latx-x86_64`` and ``latx-i386`` are present beside the
  manager.  The output is the stable two-line interface introduced with the
  manager::

    translator_x86_64=present
    translator_i386=present

  ``--root`` inspects ``HOST_ROOT/usr/bin`` without executing a translator.
  This is an offline host installation check; it does not inspect a guest
  runtime root.

``current``
  Ask the translators for their effective ``LAT_LD_PREFIX`` and the source
  that selected it.  By default both ABIs are reported; ``--abi`` limits the
  query to one.  Without ``--program``, only global configuration is applied.
  With ``--program``, each translator also applies a matching per-program
  configuration section.  This command treats *PROGRAM* as a configuration
  key and does not open or execute it.  The manager consumes the translator's
  versioned ``--runtime-info`` contract instead of reimplementing LATX
  configuration precedence.

``list``
  Query both translators and print one query result per ABI.  This reports the
  runtime roots currently selected by the installed translators; it does not
  scan the filesystem or claim to enumerate every rootfs stored on the
  machine.  A later runtime store can add discoverable installed runtimes
  without changing the meaning of these query results.

``inspect-root``
  Inspect an explicit guest runtime root.  Without ``--abi``, both ABI loader
  paths are checked.  The canonical loader paths are
  ``/lib64/ld-linux-x86-64.so.2`` for ``x86_64`` and
  ``/lib/ld-linux.so.2`` for ``i386``.

  A ready loader must resolve inside the runtime root, be a regular readable
  ELF file with the expected class and machine, and contain a loadable
  segment.  A symlink that resolves outside the runtime root is reported as
  ``unknown`` instead of being followed as a host file.  Use ``--`` before a
  root whose name begins with ``-``.

  This self-contained-runtime check is intentionally stricter than LAT's host
  path fallback: a loader found outside the selected root is never accepted as
  proof that the root itself is ready.

``doctor``
  Perform the complete read-only chain: find the translator, query its
  effective runtime selection, and inspect the loader LAT would select.
  Without ``--abi`` or ``--program``, both ABIs are checked independently.

  With ``--program``, the manager opens the ELF file without executing it,
  reads its ABI and ``PT_INTERP``, and checks only the matching translator.  If
  ``--abi`` is also present, it must match the file.  A static ELF needs no
  interpreter and is reported ready once its translator query succeeds.  If
  the file cannot be inspected, the manager returns an ``unknown`` record with
  ``guest_abi`` set to null and a stable ``program_reason`` before exiting.

  For a dynamic ELF, ``runtime_root_status`` describes the configured copy of
  its actual ``PT_INTERP`` path.  ``effective_loader_status`` and ``readiness``
  describe the loader LAT would use: the configured-root copy when it exists,
  otherwise LAT's absolute host-path fallback.  An existing but invalid
  configured loader blocks that fallback.  The ``readiness`` field is one of:

  ``ready``
    The translator selected a runtime with a valid loader.

  ``ready_with_host_fallback``
    The configured root lacks the requested loader, but LAT's host-path
    fallback resolves to a valid loader.  This is executable according to the
    current lookup rules, but is not a self-contained runtime root.

  ``unavailable``
    This ABI's translator is not installed.  When the other ABI is ready this
    does not make the overall diagnosis fail.

  ``broken``
    The selection was obtained, but the root or loader is missing or invalid.

  ``unknown``
    The manager could not safely complete the query, for example because the
    translator returned an unsupported runtime-information contract or a
    loader escaped the root.

Exit status
-----------

``status`` keeps its compatibility behavior and returns zero after a
successful inspection even when a translator is absent.  The JSON query
commands use the following statuses:

``0``
  The requested query completed and is healthy.  For default ``current`` and
  ``list`` queries, at least one ABI is selected and any other ABI is merely
  unavailable.  For the default dual-ABI ``doctor``, at least one ABI is ready
  and any other ABI is merely unavailable.  ``inspect-root`` remains a check
  of every ABI requested, so use ``--abi`` for an intentionally single-ABI
  root.

``1``
  The result is known but unavailable, missing, or invalid.

``2``
  The command line is invalid or the result cannot be determined safely.
