#!/usr/bin/env python3
"""Deliver SIGILL to a busy guest, then check its synchronous UD2 handler."""

import ctypes
import os
from pathlib import Path
import resource
import select
import shutil
import signal
import subprocess
import sys
import tempfile
import time


def disable_core():
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))


def run_case(emulator, guest, delivery, libc):
    env = dict(os.environ, LATX_AOT="0", LATX_KZT="0")
    with subprocess.Popen(
        [emulator, str(guest), delivery], stdout=subprocess.PIPE,
        stderr=subprocess.PIPE, env=env, preexec_fn=disable_core,
    ) as child:
        try:
            readable, _, _ = select.select([child.stdout], [], [], 10)
            if not readable or child.stdout.readline() != b"READY\n":
                raise RuntimeError(f"{delivery}: guest did not become ready")
            # Leave the write syscall before sending. The guest handler also
            # checks that the interrupted guest PC belongs to its busy loop.
            time.sleep(0.1)
            if delivery == "kill":
                os.kill(child.pid, signal.SIGILL)
            elif libc.tgkill(child.pid, child.pid, signal.SIGILL) != 0:
                raise OSError(ctypes.get_errno(), "tgkill")
            _, stderr = child.communicate(timeout=10)
            if child.returncode != 0:
                raise RuntimeError(
                    f"{delivery}: guest returncode={child.returncode}; "
                    f"stderr={stderr.decode(errors='replace')}"
                )
            print(f"PASS: {delivery} SIGILL and guest UD2 handlers completed")
        finally:
            if child.poll() is None:
                child.kill()
                child.wait()


def main():
    emulator, source, arch = sys.argv[1:]
    libc = ctypes.CDLL(None, use_errno=True)
    if not hasattr(libc, "tgkill"):
        print("SKIP: libc tgkill is required")
        return 77
    libc.tgkill.argtypes = [ctypes.c_int, ctypes.c_int, ctypes.c_int]
    libc.tgkill.restype = ctypes.c_int

    with tempfile.TemporaryDirectory(prefix="lat-async-sigill-") as tmp:
        guest_dir = os.environ.get("LATX_SIGILL_GUEST_DIR")
        if guest_dir:
            guest = Path(guest_dir) / f"async-sigill-{arch}"
            if not guest.is_file():
                raise RuntimeError(f"missing prebuilt guest: {guest}")
        else:
            compiler = shutil.which("clang-19") or shutil.which("clang")
            if not compiler:
                print("SKIP: clang is required to build the guest")
                return 77
            guest = Path(tmp) / f"async-sigill-{arch}"
            subprocess.run([
                compiler, f"--target={arch}-linux-gnu", "-fuse-ld=lld",
                "-nostdlib", "-static", "-Wl,--build-id=none", source,
                "-o", str(guest),
            ], check=True)
        for delivery in ("kill", "tgkill"):
            run_case(emulator, guest, delivery, libc)
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, RuntimeError, subprocess.SubprocessError) as error:
        print(f"FAIL: {error}", file=sys.stderr)
        sys.exit(1)
