#!/usr/bin/env python3
"""Unrelated guest closes must not wait for a stalled TB submission."""
import json
import os
from pathlib import Path
import socket
import subprocess
import sys
import threading

runner, runtime, rootfs, work = sys.argv[1:]
work = Path(work)
work.mkdir(parents=True, exist_ok=True)
guest = '''import os,sys,threading,time
fd=os.open('/dev/null',os.O_RDONLY)
if sys.argv[1]=='source-close':
 sources=[]
 for name in os.listdir('/proc/self/fd'):
  try:
   if int(name)>=3 and 'libc.so' in os.readlink('/proc/self/fd/'+name):
    sources.append(int(name))
  except OSError: pass
 assert sources, 'no source descriptors'
 started=threading.Event()
 def close_source():
  started.set()
  for source in sources:
   os.close(source)
 worker=threading.Thread(target=close_source,daemon=True)
 worker.start()
 assert started.wait(5)
time.sleep(.3)
if sys.argv[1]=='source-close':
 assert worker.is_alive(), 'source close did not overlap submission'
t=time.monotonic()
os.close(fd)
elapsed=time.monotonic()-t
print(elapsed,flush=True)
os._exit(0)
'''

for mode in ('ordinary', 'source-close'):
    path = work / (mode + '.sock')
    clients = []
    stop = threading.Event()
    server = socket.socket(socket.AF_UNIX, socket.SOCK_SEQPACKET)
    server.bind(str(path))
    server.listen(128)
    server.settimeout(.1)

    def accept_requests():
        while not stop.is_set():
            try:
                client, _ = server.accept()
                clients.append(client)
            except socket.timeout:
                pass

    worker = threading.Thread(target=accept_requests)
    worker.start()
    env = {k: v for k, v in os.environ.items()
           if not k.startswith(('LATC_', 'LATX_', 'LAT_LD_'))}
    env.update(LD_LIBRARY_PATH=runtime, LAT_LD_PREFIX=rootfs, LATX_AOT='0',
               LATC_DISABLE_PRETRANSLATE='1',
               LATX_AOT_V2_LATCD_SOCKET=str(path))
    command = [runner, '-L', rootfs, rootfs+'/usr/bin/python3', '-S',
               '-c', guest, mode]
    try:
        result = subprocess.run(command, env=env, capture_output=True,
                                text=True, timeout=60)
        (work / (mode + '.json')).write_text(json.dumps(dict(
            command=command, returncode=result.returncode,
            stdout=result.stdout, stderr=result.stderr,
            connections=len(clients)), indent=2))
        assert result.returncode == 0, result.stderr
        assert clients, 'no background submission reached the stalled daemon'
        elapsed = float(result.stdout.strip())
        assert elapsed < 1, f'{mode}: close blocked for {elapsed:.3f}s'
        print(f'{mode}: close={elapsed:.6f}s', flush=True)
    finally:
        stop.set()
        worker.join()
        server.close()
        for client in clients:
            client.close()
        path.unlink()
print('test-aot-v2-close-latency: PASS')
