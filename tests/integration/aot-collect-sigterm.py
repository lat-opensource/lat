#!/usr/bin/env python3
"""Compare exit-triggered collection, signal status and generated-cache reuse."""
import hashlib,json,os,pathlib,resource,struct,subprocess,sys,tempfile,time
translator,controller=map(os.path.abspath,sys.argv[1:3])
legacy='--legacy' in sys.argv
restricted='--restricted' in sys.argv
artifact=os.environ.get('AOT_EXIT_TEST_OUTPUT')
temporary=None
if artifact:
 root=pathlib.Path(artifact);root.mkdir(parents=True,exist_ok=False)
else:
 temporary=tempfile.TemporaryDirectory(prefix='aot-exit-');root=pathlib.Path(temporary.name)
text=bytes.fromhex('488b7424100fb61e80fb6e0f843a01000080fb670f842501000080fb660f849700000080fb750f849700000080fb7a0f849500000080fb6b745180fb74745380fb6d0f848f00000080fb690f840601000080fb680f84fd00000080fb780f846401000080fb790f845b01000089de83ee41b8270000000f0589c7b83e0000000f05bf60000000e9c5000000be09000000ebdfb8270000000f0589c7b8ba0000000f0589c6ba09000000b8ea0000000f05bf60000000e99600000031c08b00e9880000000f0be98100000031d2b80100000031c9f7f1eb74b80900000031ffbe00200000ba0300000041ba2200000049c7c0ffffffff4531c90f05488db000200000bf000f050031d24531d24531c0b8380000000f054885c0740431ffeb324883ec1048c704240100000048c7442408000000004889e731f6b8230000000f05bf25000000b8e70000000f05bf25000000b83c0000000f054883ec2048c704240100000080fb69740b488d05500000004889042448c744240800000004488d053d000000488944241048c744241800000000bf060000004889e631d241ba08000000b80d0000000f05b8270000000f0589c7be06000000b83e0000000f05eb8cc3b80f0000000f05b89d000000bf26000000be0100000031d24531d24531c00f054883ec2048c7042401000000488d4424104889442408b80600000080fb78740a48b806000000000000804889442410b83d010000bf0100000031f64889e20f05b8270000000f05e91fffffff')
size=4096+len(text);guest=root/'guest'
header=struct.pack('<16sHHIQQQIHHHHHH',b'\x7fELF\x02\x01\x01'+bytes(9),2,62,1,0x401000,64,0,0,64,56,1,0,0,0)
phdr=struct.pack('<IIQQQQQQ',1,5,0,0x400000,0x400000,size,size,4096)
guest.write_bytes((header+phdr).ljust(4096,b'\0')+text);guest.chmod(0o700)
def no_core():resource.setrlimit(resource.RLIMIT_CORE,(0,0))
cases=[('seccomp-kill-thread','x',-31),('seccomp-kill-process','y',-31),('ignored-abort','i',37),('handled-abort','h',37),('exit','n',37),('exit-group','g',37),('thread-exit','m',37),('fault-segv','f',-11),('fault-ill','u',-4),('fault-fpe','z',-8),('self-kill9','k',-9),('self-tgkill9','t',-9)]
cases += [('signal-%d'%sig,chr(65+sig),-sig) for sig in [1,2,3,4,5,6,7,8,10,11,12,13,14,15,24,25,26,27,29,30,31]]
rows=[]
try:
 for controlled in ([False] if legacy else [False,True]):
  for name,mode,raw in cases:
   home=root/('%s-%s'%('controller' if controlled else 'direct',name));home.mkdir()
   env={k:v for k,v in os.environ.items() if not k.startswith(('LATX_','LAT_'))}
   env.update(HOME=str(home),LATX_FAST_TRANSLATOR='0' if legacy else '1',LATX_FAST_TRANSLATOR_STRICT='0' if legacy else '1',LATX_KZT='0',LATX_AOT='1' if legacy else '0',LATX_LOAD_AOT='0',LATX_AOT_GENERATE='1' if legacy else '0',LATX_QUICKTRANS_AOT_COLLECT='0' if legacy else '1')
   report=home/'report.json';cmd=([controller,'--report',str(report),'--'] if controlled else [])+[translator,str(guest),mode]
   run=subprocess.run(cmd,env=env,stdout=subprocess.PIPE,stderr=subprocess.PIPE,timeout=25,preexec_fn=no_core)
   (home/'stdout').write_bytes(run.stdout);(home/'stderr').write_bytes(run.stderr)
   expected=name!='seccomp-kill-process' and (not restricted or raw==37 or raw==-15)
   deadline=time.monotonic()+(5 if expected and legacy else 0)
   while not list(home.rglob('*.aot2')) and time.monotonic()<deadline:time.sleep(.02)
   caches=list(home.rglob('*.aot2'));rc=128-raw if controlled and raw<0 else raw
   row={'name':name,'controlled':controlled,'rc':run.returncode,'expected_rc':rc,'cache_files':len(caches),'expected_cache':expected,'binary_sha256':hashlib.sha256(pathlib.Path(translator).read_bytes()).hexdigest()};rows.append(row)
   assert run.returncode==rc,(row,run.stderr)
   assert bool(caches)==expected,(row,run.stderr)
   if controlled:
    result=json.loads(report.read_text());row['worker_count']=len(result['workers'])
    assert result['app_status']==rc and result['failures']==0,result
    assert bool(result['workers'])==expected,result
    assert all(w['status']==0 and w['reaped'] for w in result['workers']),result
    if name=='thread-exit' and not restricted:assert len(result['workers'])==2,result
    if name=='exit':assert len(result['workers'])==1,result
   if caches:
    marker=home/'loaded';env.update(LATX_FAST_TRANSLATOR='1',LATX_FAST_TRANSLATOR_STRICT='1',LATX_QUICKTRANS_AOT_COLLECT='0',LATX_AOT='1',LATX_LOAD_AOT='1',LATX_AOT_GENERATE='0',LATX_TEST_AOT_LOAD_MARKER=str(marker))
    reuse=subprocess.run([translator,str(guest),'n'],env=env,stdout=subprocess.PIPE,stderr=subprocess.PIPE,timeout=15,preexec_fn=no_core)
    (home/'reuse.stderr').write_bytes(reuse.stderr)
    assert reuse.returncode==37 and marker.exists() and marker.stat().st_size,(row,reuse.returncode,reuse.stderr)
    row['cache_loaded']=True
   print('PASS',name,'controller' if controlled else 'direct','rc',run.returncode,'cache',len(caches),flush=True)
 # Collection disabled must not generate on normal or signal exit.
 if not legacy:
  for mode,rc in [('n',37),('G',-6)]:
   home=root/('disabled-'+mode);home.mkdir()
   env.update(HOME=str(home),LATX_QUICKTRANS_AOT_COLLECT='0',LATX_AOT='0',LATX_LOAD_AOT='0',LATX_AOT_GENERATE='0')
   run=subprocess.run([translator,str(guest),mode],env=env,stdout=subprocess.PIPE,stderr=subprocess.PIPE,timeout=15,preexec_fn=no_core)
   assert run.returncode==rc and not list(home.rglob('*.aot2')),(mode,run.returncode,run.stderr)
   rows.append({'name':'disabled-'+mode,'rc':run.returncode,'cache_files':0})
   print('PASS disabled',mode,rc,flush=True)
 # An external SIGKILL has no guest syscall hook in either translation path.
 spin=root/'spin';spin.write_bytes(guest.read_bytes()[:4096]+b'\xeb\xfe'+bytes(len(text)-2));spin.chmod(0o700)
 home=root/'external-kill';home.mkdir()
 env.update(HOME=str(home),LATX_FAST_TRANSLATOR='0' if legacy else '1',LATX_FAST_TRANSLATOR_STRICT='0' if legacy else '1',LATX_AOT='1' if legacy else '0',LATX_LOAD_AOT='0',LATX_AOT_GENERATE='1' if legacy else '0',LATX_QUICKTRANS_AOT_COLLECT='0' if legacy else '1')
 child=subprocess.Popen([translator,str(spin)],env=env,stdout=subprocess.PIPE,stderr=subprocess.PIPE,preexec_fn=no_core)
 try:
  time.sleep(.3)
  assert child.poll() is None,'external-kill fixture did not stay running'
  child.kill();stdout,stderr=child.communicate(timeout=5)
  assert child.returncode==-9 and not list(home.rglob('*.aot2')),(child.returncode,stderr)
  rows.append({'name':'external-kill','rc':child.returncode,'cache_files':0})
  print('PASS external SIGKILL -9 no cache',flush=True)
 finally:
  if child.poll() is None:child.kill();child.wait()
finally:
 (root/'results.json').write_text(json.dumps(rows,indent=2))
 if temporary:temporary.cleanup()
