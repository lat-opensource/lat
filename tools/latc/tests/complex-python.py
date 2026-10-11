import ctypes
import hashlib
import os
import signal
import sqlite3
import ssl
import subprocess
import sys
import tempfile
import threading


def trace(stage):
    if os.environ.get("LATC_COMPLEX_TRACE"):
        print("PYTHON_TRACE", stage, file=sys.stderr, flush=True)


trace("imports")
assert ssl.OPENSSL_VERSION
assert sqlite3.sqlite_version
assert ctypes.CDLL(None).getpid() == os.getpid()
trace("basic-checks")

signal_seen = threading.Event()


def handle_signal(_signum, _frame):
    signal_seen.set()


signal.signal(signal.SIGUSR1, handle_signal)
os.kill(os.getpid(), signal.SIGUSR1)
assert signal_seen.wait(2)
trace("signal")

results = [None] * 8


def calculate(index):
    payload = bytes((value + index) & 0xFF for value in range(20000))
    results[index] = hashlib.sha256(payload).hexdigest()


threads = [threading.Thread(target=calculate, args=(index,))
           for index in range(len(results))]
trace("threads-created")
for thread in threads:
    thread.start()
trace("threads-started")
for thread in threads:
    thread.join()
assert all(results) and len(set(results)) == len(results)
trace("threads-joined")

with tempfile.TemporaryDirectory() as directory:
    path = os.path.join(directory, "payload")
    with open(path, "wb") as stream:
        stream.write("复杂应用".encode() * 100)
    with open(path, "rb") as stream:
        digest = hashlib.sha256(stream.read()).hexdigest()
    assert len(digest) == 64
trace("temporary-file")

runner = os.environ["LATC_COMPLEX_RUNNER"]
rootfs = os.environ["LATC_COMPLEX_ROOTFS"]
child = subprocess.run([runner, "-L", rootfs, sys.executable, "-c",
                        "import threading; print('PY_CHILD_OK', threading.active_count())"],
                       check=True, text=True, stdout=subprocess.PIPE)
assert child.stdout.strip() == "PY_CHILD_OK 1", child
trace("child")
print("PYTHON_OK", sys.version.split()[0], sqlite3.sqlite_version,
      len(ssl.OPENSSL_VERSION), digest[:16])
