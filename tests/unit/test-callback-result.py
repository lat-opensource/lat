#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Run production format dispatch and status thunks with injected scope failures."""
import argparse
import os
import re
from pathlib import Path
import shlex
import subprocess
import tempfile


def function(source, signature):
    start = source.index(signature)
    return source[start:source.index('\n}', start) + 2] + '\n'


def macro(source, name, expansion):
    start = source.index('#define ' + name + '(')
    end = source.index('\n' + expansion + '(', start)
    return source[start:end] + '\n' + name + '(0)\n'


PRELUDE = r'''
#include <assert.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "callback-args.h"
#include "callback.h"
#define CHECK(c) do { if (!(c)) { fprintf(stderr,"FAIL:%d: %s\n",__LINE__,#c); exit(1); } } while(0)
#define TARGET_X86_64 1
#define CODEIS64 1
#define lsassert assert
#define g_assert assert
#define LATX_GUEST_USER_CALLBACK 0
#define ZMM_Q(n) q[n]
enum { R_EDI, R_ESI, R_EDX, R_ECX, R_R8, R_R9, R_ESP };
static const int callback_gpr_regs[6] = { R_EDI,R_ESI,R_EDX,R_ECX,R_R8,R_R9 };
typedef struct { uint64_t regs[16]; struct { uint64_t q[2]; } xmm_regs[8]; } CPU;
static CPU cpu;
static uint64_t stack[32], guest_value;
static unsigned guest_runs, leaves, clones;
static bool fail_scope, tls_enabled = true;
typedef struct { bool active; } CallbackScope;
typedef struct { CPU *cpu; } CallbackFrame;
static int callback_scope_enter(CallbackScope *s, int kind) {
    (void)kind; s->active=true;
    return tls_enabled && fail_scope ? -1 : 0;
}
static void callback_scope_leave(CallbackScope *s) {
    if (s->active) { s->active=false; leaves++; cpu.regs[15]=0xbad; }
}
static CallbackFrame callback_frame_enter(size_t n, bool aligned) {
    (void)aligned; CHECK(n<=32); cpu.regs[R_ESP]=(uintptr_t)stack;
    return (CallbackFrame){&cpu};
}
static uint64_t callback_frame_run(CallbackFrame *f, uintptr_t entry) {
    (void)f; CHECK(entry==1); guest_runs++; return guest_value;
}
enum { CAIRO_STATUS_SUCCESS=0, CAIRO_STATUS_NO_MEMORY=1 };
static uintptr_t cairo_io_slots[1]={1}, cairo_user_init_slots[1]={1},
    cairo_user_render_slots[1]={1}, cairo_user_text_slots[1]={1},
    cairo_user_unicode_slots[1]={1}, cairo_raster_snapshot_slots[1]={1},
    cairo_raster_copy_slots[1]={1};
static uintptr_t cairo_slot_guest(uintptr_t *slots, unsigned n) { return slots[n]; }
static bool cairo_binding_clone_raster(void *a, void *b) {
    (void)a; (void)b; clones++; return true;
}
'''

CAIRO_CASES = {
    'DEFINE_IO_THUNK': 'cairo_io_thunk_0(NULL,NULL,0)',
    'DEFINE_USER_INIT_THUNK': 'cairo_user_init_thunk_0(NULL,NULL,NULL)',
    'DEFINE_USER_RENDER_THUNK': 'cairo_user_render_thunk_0(NULL,0,NULL,NULL)',
    'DEFINE_USER_TEXT_THUNK': 'cairo_user_text_thunk_0(NULL,NULL,0,NULL,NULL,NULL,NULL,NULL)',
    'DEFINE_USER_UNICODE_THUNK': 'cairo_user_unicode_thunk_0(NULL,0,NULL)',
    'DEFINE_RASTER_SNAPSHOT_THUNK': 'cairo_raster_snapshot_thunk_0(NULL,NULL)',
    'DEFINE_RASTER_COPY_THUNK': 'cairo_raster_copy_thunk_0(NULL,NULL,NULL)',
}

FT_CASES = {
    'DEFINE_MOVE_THUNK': 'ft_move_thunk_0(NULL,NULL)',
    'DEFINE_LINE_THUNK': 'ft_line_thunk_0(NULL,NULL)',
    'DEFINE_CONIC_THUNK': 'ft_conic_thunk_0(NULL,NULL,NULL)',
    'DEFINE_CUBIC_THUNK': 'ft_cubic_thunk_0(NULL,NULL,NULL,NULL)',
    'DEFINE_LIST_ITERATOR_THUNK': 'ft_list_iterator_thunk_0(NULL,NULL)',
    'DEFINE_FACE_REQUESTER_THUNK': 'ft_face_requester_thunk_0(NULL,NULL,NULL,&face)',
}
FT_PRELUDE = r'''
enum { FT_ERROR_SUCCESS=0, FT_ERROR_INVALID_ARGUMENT=6,
       FT_ERROR_UNIMPLEMENTED_FEATURE=7, FT_ERROR_OUT_OF_MEMORY=64 };
static uintptr_t ft_move_slots[1]={1}, ft_line_slots[1]={1},
    ft_conic_slots[1]={1}, ft_cubic_slots[1]={1}, ft_list_iterator_slots[1]={1},
    ft_face_requester_slots[1]={1};
static int freetype_lock;
static void g_mutex_lock(int *m) { (void)m; }
static void g_mutex_unlock(int *m) { (void)m; }
static uintptr_t freetype_slot_guest(uintptr_t *slots,unsigned n) { return slots[n]; }
static bool freetype_generic_patch_face_locked(void *a,void *b,bool c) {
    (void)a;(void)b;(void)c;clones++;return true;
}
static void my_FT_Done_Face(void *face) { (void)face; abort(); }
typedef struct { void *read, *close; } FtStreamAbi;
typedef struct { FtStreamAbi *guest; FtStreamAbi native;
                 void *callbacks[2], *native_callbacks[2]; } FtStreamBridge;
typedef uintptr_t (*FtStreamIoFuncAbi)(FtStreamAbi *,uintptr_t,uint8_t *,uintptr_t);
static uintptr_t freetype_stream_read(FtStreamAbi *,uintptr_t,uint8_t *,uintptr_t);
static void freetype_stream_close(FtStreamAbi *s) { (void)s; }
'''

MAIN = r'''
int main(void) {
    void *face=(void *)1;
#ifdef HAVE_CHECKED
    uint64_t result=0xfeed;
    unsigned before;
    fail_scope=true;
    CHECK(RunFunctionFmtChecked(1,&result,"p",NULL)==-1);
    CHECK(result==0xfeed && guest_runs==0 && leaves==1);
    CHECK(RunFunctionFmtChecked(0,&result,"")==-1 && result==0xfeed);
    CHECK(RunFunctionFmtChecked(1,NULL,"")==-1);
    fail_scope=false;
    guest_value=0;
    CHECK(RunFunctionFmtChecked(1,&result,"")==0 && result==0);
    guest_value=UINT64_C(0xfedcba9876543210);
    CHECK(RunFunctionFmtChecked(1,&cpu.regs[15],"")==0);
    CHECK(cpu.regs[15]==guest_value); /* Cleanup must precede output publication. */
    CHECK(RunFunctionFmt(1,"")==guest_value);
    CHECK(RunFunctionFmtChecked(1,&result,"pppppppdd",
          (void *)1,(void *)2,(void *)3,(void *)4,(void *)5,(void *)6,
          (void *)7,1.5,2.5)==0);
    CHECK(cpu.regs[R_EDI]==1 && cpu.regs[R_R9]==6 && stack[0]==7);
    double number;
    memcpy(&number,&cpu.xmm_regs[0].q[0],sizeof(number)); CHECK(number==1.5);
    memcpy(&number,&cpu.xmm_regs[1].q[0],sizeof(number)); CHECK(number==2.5);
    before=guest_runs;
    result=0xfeed;
    CHECK(RunFunctionFmtChecked(1,&result,"?")==-1);
    CHECK(result==0xfeed && guest_runs==before);
#endif
    for (unsigned tls=0; tls<2; tls++) {
        tls_enabled=tls;
        fail_scope=false; guest_value=0;
        CHECK(RunFunctionFmt(1,"p",NULL)==0);
#ifdef HAVE_CHECKED
        CHECK(RunFunctionFmtChecked(1,&result,"p",NULL)==0 && result==0);
#endif
    }
    tls_enabled=true;
#ifdef HAVE_FALLBACK
    fail_scope=true; guest_runs=0;
    CHECK(RunFunctionFmt(1,"")==0 && guest_runs==0);
    CHECK(RunFunctionFmtOr(1,UINT64_MAX,"")==UINT64_MAX && guest_runs==0);
    CHECK(RunFunctionFmtOr(0,123,"")==123 && guest_runs==0);
    CHECK(RunFunctionFmtOr(1,123,NULL)==123 && guest_runs==0);
    fail_scope=false; guest_value=0;
    CHECK(RunFunctionFmtOr(1,123,"")==0);
    guest_value=123;
    CHECK(RunFunctionFmtOr(1,123,"")==123); /* A real Guest error is preserved. */
    CHECK(RunFunctionFmtOr(1,456,"?")==456);
#endif
CAIRO_TESTS
    puts("PASS: callback execution/result separation and status consumers");
    return 0;
}
'''


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('repo', type=Path)
    parser.add_argument('--revision')
    parser.add_argument('--core-only', action='store_true')
    parser.add_argument('--consumer', choices=('all','cairo','freetype'), default='all')
    args = parser.parse_args()

    def read(path):
        if args.revision:
            return subprocess.check_output(['git', '-C', str(args.repo), 'show',
                                            args.revision + ':' + path], text=True)
        return (args.repo / path).read_text()

    callback = read('target/i386/latx/context/callback.c')
    cairo = read('target/i386/latx/context/wrappedcairo.c')
    freetype = read('target/i386/latx/context/wrappedfreetype.c')
    source = PRELUDE + FT_PRELUDE
    if 'run_function_fmt_va(' in callback:
        source += '#define HAVE_CHECKED 1\n'
        signature = re.search(
            r'static(?: inline)?(?: __attribute__\(\(always_inline\)\))? '
            r'(?:int|uint64_t)\s+run_function_fmt_va\(', callback).group(0)
        source += function(callback, signature)
        if 'uint64_t RunFunctionFmtOr(' in callback:
            source += '#define HAVE_FALLBACK 1\n'
            source += function(callback, 'uint64_t RunFunctionFmtOr(')
        source += function(callback, 'uint64_t RunFunctionFmt(')
        source += function(callback, 'int RunFunctionFmtChecked(')
    else:
        source += function(callback, 'uint64_t RunFunctionFmt(')
    checks = ''
    if not args.core_only:
        for name, call in (CAIRO_CASES if args.consumer != 'freetype' else {}).items():
            source += macro(cairo, name, 'CAIRO_CALLBACK_SLOT_LIST')
            checks += f'''    fail_scope=true; guest_value=0; guest_runs=0; clones=0;
    CHECK({call} != 0 && guest_runs==0 && clones==0);
    fail_scope=false;
    CHECK({call} == 0 && guest_runs==1);
    guest_value=11;
    CHECK({call} == 11 && guest_runs==2);
'''
    if not args.core_only and args.consumer != 'cairo':
        for name, call in FT_CASES.items():
            source += macro(freetype, name, 'FT_SLOT_LIST')
            checks += f'''    fail_scope=true; guest_value=0; guest_runs=0; clones=0;
    CHECK({call} != 0 && guest_runs==0 && clones==0);
    fail_scope=false;
    CHECK({call} == 0 && guest_runs==1);
    guest_value=11;
    CHECK({call} == 11 && guest_runs==2);
'''
        for signature in ('static void freetype_stream_guest_view(',
                          'static void freetype_stream_native_view(',
                          'static uintptr_t freetype_stream_read('):
            # Skip forward declarations when extracting the production body.
            start = freetype.index(signature)
            while freetype.find(';',start,freetype.index('{',start)) >= 0:
                start = freetype.index(signature,start+len(signature))
            source += function(freetype[start:],signature)
        checks += r'''
    FtStreamAbi stream={0};
    FtStreamBridge bridge={.guest=&stream,.callbacks={(void *)1,NULL}};
    fail_scope=true; guest_runs=0;
    CHECK(freetype_stream_read(&bridge.native,0,NULL,0)!=0 && guest_runs==0);
    CHECK(freetype_stream_read(&bridge.native,0,NULL,4)==0 && guest_runs==0);
    CHECK(bridge.native.read==(void *)freetype_stream_read);
    fail_scope=false;guest_value=0;
    CHECK(freetype_stream_read(&bridge.native,0,NULL,0)==0);
    guest_value=3;
    CHECK(freetype_stream_read(&bridge.native,0,NULL,4)==3);
'''
    source += MAIN.replace('CAIRO_TESTS', checks)
    with tempfile.TemporaryDirectory(prefix='lat-callback-result-') as directory:
        src = Path(directory) / 'test.c'
        src.write_text(source)
        for defines in ([], ['-DNDEBUG']):
            executable = Path(directory) / 'test'
            command = shlex.split(os.environ.get('CC', 'cc')) + [
                '-std=gnu11', '-O2', '-Wall', '-Wextra', '-Wno-unused-function',
                '-Wno-unused-variable', '-Werror', *defines,
                '-I' + str(args.repo / 'target/i386/latx/context'),
                '-I' + str(args.repo / 'target/i386/latx/include'),
                str(src), str(args.repo / 'target/i386/latx/context/callback-args.c'),
                '-o', str(executable)]
            subprocess.run(command, check=True)
            subprocess.run([str(executable)], check=True)


if __name__ == '__main__':
    main()
