/*
 *  exit support for qemu
 *
 *  Copyright (c) 2018 Alex Bennée <alex.bennee@linaro.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, see <http://www.gnu.org/licenses/>.
 */
#include "qemu/osdep.h"
#include "qemu.h"
#ifdef CONFIG_GPROF
#include <sys/gmon.h>
#endif

#ifdef CONFIG_GCOV
extern void __gcov_dump(void);
#endif
#ifdef CONFIG_LATX_DEBUG
#include "latx-config.h"
#include "latx-options.h"
#include "tcg/tcg.h"

#endif

#ifdef CONFIG_LATX_PERF
#include "latx-perf.h"
#endif

#ifdef CONFIG_LATX_KZT
#include "library.h"
#endif

void preexit_cleanup(CPUArchState *env, int code)
{
#ifdef CONFIG_LATX_PERF
    latx_timer_stop(TIMER_PROCESS);
#endif
#ifdef CONFIG_LATX_PERF
    latx_print_all_timers();
#endif

#ifdef CONFIG_GPROF
        _mcleanup();
#endif
#ifdef CONFIG_GCOV
        __gcov_dump();
#endif
#ifdef CONFIG_LATX_PROFILER
        if (qemu_loglevel_mask(LAT_LOG_PROFILE))
            dump_exec_info();
#endif
#ifdef CONFIG_LATX_KZT
    if (option_kzt) {
        FreeLoadedLibs();
    }
#endif
        gdb_exit(code);
        qemu_plugin_atexit_cb();
}
