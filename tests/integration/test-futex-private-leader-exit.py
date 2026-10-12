#!/usr/bin/env python3
"""Exercise real private-futex recovery after an i386 process leader exits."""
import os
from pathlib import Path
import select
import shutil
import struct
import subprocess
import sys
import tempfile
import time

PASS = b"PASS: all four private waits retried after leader exit\n"


def main():
    emulator, source, shim_source = map(os.path.abspath, sys.argv[1:])
    with open(emulator, "rb") as binary:
        header = binary.read(64)
        if header[:6] != b"\x7fELF\x02\x01":
            raise RuntimeError("expected a little-endian ELF64 translator")
        phoff = struct.unpack_from("<Q", header, 32)[0]
        phsize, phnum = struct.unpack_from("<HH", header, 54)
        dynamic = False
        for index in range(phnum):
            binary.seek(phoff + index * phsize)
            dynamic |= binary.read(4) == struct.pack("<I", 3)  # PT_INTERP
        if not dynamic:
            print("SKIP: the mremap shim requires a dynamic translator")
            return 77
    cc = shutil.which(os.environ.get("CC", "cc"))
    if not cc:
        print("SKIP: a host C compiler is required")
        return 77
    with tempfile.TemporaryDirectory(prefix="lat-futex-leader-") as tmp:
        fixture = os.environ.get("LATX_FUTEX_LEADER_FIXTURE")
        if fixture:
            guest = Path(fixture).resolve()
            if not guest.is_file():
                raise RuntimeError(f"missing prebuilt guest: {guest}")
        else:
            clang = shutil.which("clang-19") or shutil.which("clang")
            if not clang or not shutil.which("ld.lld"):
                print("SKIP: need Clang/LLD or LATX_FUTEX_LEADER_FIXTURE")
                return 77
            guest = Path(tmp) / "guest"
            subprocess.run([
                clang, "--target=i386-linux-gnu", "-fuse-ld=lld",
                "-nostdlib", "-static", "-no-pie", "-O2", "-ffreestanding",
                "-fno-builtin", "-fno-pie", "-fno-stack-protector",
                "-Wl,--build-id=none", source, "-o", str(guest),
            ], check=True)
        shim = Path(tmp) / "shim.so"
        subprocess.run([
            cc, "-shared", "-fPIC", "-O2", "-Wall", "-Wextra",
            shim_source, "-ldl", "-o", str(shim),
        ], check=True)
        env = dict(os.environ, LATX_AOT="0", LATX_KZT="0",
                   LD_PRELOAD=str(shim))
        with subprocess.Popen(
            [emulator, str(guest)], stdin=subprocess.PIPE,
            stdout=subprocess.PIPE, stderr=subprocess.PIPE, env=env,
        ) as child:
            try:
                readable, _, _ = select.select([child.stdout], [], [], 10)
                if not readable or child.stdout.readline() != b"READY\n":
                    raise RuntimeError("guest did not create both workers")
                deadline = time.monotonic() + 10
                while True:
                    stat = Path(f"/proc/{child.pid}/stat").read_text()
                    if stat.rsplit(") ", 1)[1].split()[0] == "Z":
                        break
                    if time.monotonic() >= deadline:
                        raise RuntimeError("process leader did not exit")
                    time.sleep(0.01)
                print("LEADER_EXITED: host task state Z", flush=True)
                child.stdin.write(b"x")
                child.stdin.flush()
                stdout, stderr = child.communicate(timeout=10)
                sys.stdout.buffer.write(stdout)
                sys.stderr.buffer.write(stderr)
                if child.returncode:
                    errors = {
                        20: "transient EFAULT escaped after leader exit",
                        21: "unexpected futex result",
                        22: "not all four futex recovery paths ran",
                    }
                    raise RuntimeError(errors.get(
                        child.returncode, f"guest status {child.returncode}"))
                if stdout != PASS:
                    raise RuntimeError("missing futex recovery coverage marker")
            finally:
                if child.poll() is None:
                    child.kill()
                    child.wait()
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, RuntimeError, subprocess.SubprocessError) as error:
        print(f"FAIL: {error}", file=sys.stderr)
        sys.exit(1)
