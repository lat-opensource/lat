/*
 * This file is derived from Box64.
 *
 * SPDX-FileCopyrightText: 2020 ptitSeb
 *
 * SPDX-License-Identifier: MIT
 */

#include<unistd.h>
#include "elfloader_private.h"
#include "box64context.h"
#include "librarian.h"
#include "debug.h"
#include "library.h"
#include "fileutils.h"

#if defined(CONFIG_LATX_KZT) && defined(CONFIG_LATX_DEBUG)
void AddDebugInfo(int type, char *name, unsigned long start, unsigned long end)
{
    if (!(qemu_loglevel & CPU_LOG_EXEC)) { //LAT_LOG=exec
        return;
    }
    for(int i = 0; i < my_context->latx_kzt_debugsize; i++) {
        if (start == my_context->latx_kzt_debugs[i]->map_start && start == my_context->latx_kzt_debugs[i]->map_end) {
            fprintf(stderr, "%d debug %s: file \"%s\" exited, return\n", getpid(), __func__, name);
            return;
        }
    }
    struct latx_kzt_debug *tmp = box_malloc(sizeof(struct latx_kzt_debug));
    tmp->type = type;
    tmp->name = box_malloc(strlen(name) + 1);
    strcpy(tmp->name, name);
    tmp->map_start = start;
    tmp->map_end = end;
    AddKztDebugInfo(my_context, tmp);
}
static struct latx_kzt_debug * latx_kzt_debuginfo_scan(uintptr_t x86pc)
{
    for(int i = 0; i < my_context->latx_kzt_debugsize; i++) {
        struct latx_kzt_debug * debuginfo = my_context->latx_kzt_debugs[i];
        if (x86pc >= debuginfo->map_start && x86pc <= debuginfo->map_end) {
            return debuginfo;
        }
    }
    return NULL;
}
extern int option_kzt;

void latx_kzt_debuginfo_check(void)
{
    char buf[4096];
    FILE *f;
    uintptr_t tbptr, cs_base, threadhandle, x86pc, tbflags;
    if (!option_kzt || !(qemu_loglevel & CPU_LOG_EXEC)) { //LAT_LOG=exec
        return;
    }
    QemuLogFile *logfile = qatomic_rcu_read(&qemu_logfile);
    if (logfile) {
        f = logfile->fd;
        if (fseek(f, 0L, SEEK_SET) != 0) {
            fprintf(stderr, "pid %d : %s logfile error\n", getpid(), __func__);
            return;
        }
        while(!feof(f)) {
            char* ret = fgets(buf, sizeof(buf), f);
            if (!ret) {
                break;
            }
            if (sscanf(buf, "%*d %*s %*d: %lx [%lx/%lx/%lx/%lx]", &tbptr, &cs_base, &threadhandle, &x86pc, &tbflags) == 5) {
               struct latx_kzt_debug * cur = latx_kzt_debuginfo_scan(x86pc);
                if (!cur) {
                    printf_log(LOG_NONE, "\033[31mWarning, \"%s\" pc 0x%lx file is not found!\033[m\n", buf, x86pc);
                }else if (cur->type == LIB_WRAPPED) {
                    printf_log(LOG_NONE, "\033[31mWarning, \"%s\" pc 0x%lx is wrapped file name \"%s\"!\033[m\n", buf, x86pc, cur->name);
                }
            } else {
                continue;
            }
        }
    }
}
#endif
