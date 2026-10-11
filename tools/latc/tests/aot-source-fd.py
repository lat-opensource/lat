"""Exercise guest mutation of translator source descriptors before exec."""
import ctypes
import errno
import fcntl
import os
import subprocess
import sys

runner, rootfs, mode = sys.argv[1:]
libc = ctypes.CDLL(None, use_errno=True)
targets = []
for name in os.listdir('/proc/self/fd'):
    fd = int(name)
    if fd < 3:
        continue
    try:
        target = os.readlink('/proc/self/fd/' + name)
    except FileNotFoundError:
        continue
    if target.startswith(rootfs) and ('.so' in target or '/usr/bin/python' in target):
        targets.append(fd)
assert targets, 'test did not find translator-owned source descriptors'
null = os.open('/dev/null', os.O_RDONLY)
for fd in sorted(targets):
    if mode == 'close':
        os.close(fd)
    elif mode == 'dup2':
        os.dup2(null, fd)
    elif mode == 'dup3':
        assert libc.dup3(null, fd, os.O_CLOEXEC) == fd, ctypes.get_errno()
    elif mode == 'setfd':
        fcntl.fcntl(fd, fcntl.F_SETFD, 0)
    else:
        raise AssertionError(mode)
os.close(null)

# A failed exec must preserve pending data and leave the process usable.
try:
    os.execve('/latc-fd-test-missing-executable', ['missing'], os.environ)
except OSError as error:
    assert error.errno == errno.ENOENT, error
else:
    raise AssertionError('unexpected successful exec')
assert sum(range(1000)) == 499500
child = subprocess.run([runner, '-L', rootfs, sys.executable, '-S', '-c',
                        'print("FD_CHILD_OK")'], check=True,
                       stdout=subprocess.PIPE, text=True)
assert child.stdout.strip() == 'FD_CHILD_OK', child.stdout
print('SOURCE_FD_OK', mode, len(targets))
