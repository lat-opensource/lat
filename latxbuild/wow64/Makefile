# SPDX-License-Identifier: GPL-2.0-or-later

ROOT := $(abspath ../..)
LATX := target/i386/latx
HERE := $(CURDIR)

OUT ?= $(ROOT)/build-wowlatx
WINCC ?= loong64-w64-mingw32-clang
PYTHON ?= python3
MESON ?= meson
NINJA ?= ninja
WINE_PE_BUILD ?= $(WINE_SRC)

ifeq ($(strip $(WINE_SRC)),)
$(error Set WINE_SRC to the Hangover Wine source directory)
endif

.DELETE_ON_ERROR:

GEN := $(OUT)/generated
GLIB_SRC := $(ROOT)/third_party/glib
GLIB_COPY := $(OUT)/glib-source
GLIB_BUILD := $(OUT)/glib
GLIB_LIB := $(GLIB_BUILD)/glib/libglib-2.0.a
INTL_LIB := $(GLIB_BUILD)/subprojects/proxy-libintl-c03e1a74b17fa7ec467e110130775409e4828a4c/libintl.a
CROSS := $(OUT)/loong64-windows.ini

PE_IMPORTS := $(WINE_PE_BUILD)/dlls/ntdll/loong64-windows/libntdll.a \
              $(WINE_PE_BUILD)/dlls/wow64/loong64-windows/libwow64.a
WINEBUILD := $(WINE_PE_BUILD)/tools/winebuild/winebuild
WINE_INCLUDES := -I$(WINE_SRC)/include -I$(ROOT)/include

# This profile uses the same translator sources as the Linux build. The
# translator directory contains only the common instruction generators.
CORE := $(addprefix $(LATX)/,ir1/ir1.c ir2/ir2.c ir2/ir2-assemble.c \
        ir2/ir2-relocate.c ir2/la-decode.c ir2/la-disasm.c latx-config.c \
        latx-options.c latx-runtime.c latx-signal.c mem.c wow64-pe.c \
        wow64-x86-state.c latx-disassemble-trace/capstone_git.c \
        optimization/flag-reduction.c optimization/flag-lbt.c \
        optimization/imm-cache.c optimization/ir2-optimization.c \
        optimization/ir1-optimization.c optimization/jrra.c optimization/tu.c) \
        $(patsubst $(ROOT)/%,%,$(wildcard $(ROOT)/$(LATX)/translator/*.c)) \
        $(addprefix target/i386/,helper.c tcg/fpu_helper.c tcg/cc_helper.c \
        tcg/int_helper.c tcg/seg_helper.c tcg/mem_helper.c tcg/misc_helper.c \
        tcg/bpt_helper.c tcg/excp_helper.c tcg/mpx_helper.c tcg/smm_helper.c tcg/svm_helper.c) \
        $(addprefix accel/tcg/,cpu-exec.c cpu-exec-common.c user-exec-stub.c \
        tb-flush.c tb-jump.c translate-all.c) \
        cpus-common.c tcg/tcg.c tcg/tcg-common.c disas.c disas/loongarch.c \
        crypto/aes.c fpu/softfloat.c \
        $(addprefix util/,bitmap.c bitops.c cutils.c error.c host-utils.c \
        interval-tree.c log.c notify.c oslib-win32.c qdist.c qemu-thread-win32.c qht.c rcu.c) \
        $(addprefix stubs/,iothread-lock.c replay.c icount.c)

CAPSTONE := $(addprefix $(LATX)/capstone_git/,cs.c utils.c SStream.c MCInst.c \
            MCInstrDesc.c MCInstPrinter.c MCRegisterInfo.c Mapping.c \
            $(addprefix arch/X86/,X86ATTInstPrinter.c X86Disassembler.c \
            X86DisassemblerDecoder.c X86InstPrinterCommon.c X86IntelInstPrinter.c \
            X86Mapping.c X86Module.c))

CORE_OBJS := $(addprefix $(OUT)/obj/,$(CORE:.c=.o)) $(OUT)/obj/la-append.o
CAPSTONE_OBJS := $(addprefix $(OUT)/obj/,$(CAPSTONE:.c=.o))
CPU_OBJ := $(OUT)/obj/$(LATX)/wow64-cpu.o

PE_CFLAGS := -O2 -flto -ffunction-sections -fdata-sections
PE_LDFLAGS := -shared -Wl,--exclude-all-symbols,--gc-sections,--image-base,0x7a400000
CORE_CPPFLAGS := -DNEED_CPU_H -DNDEBUG -DNOMINMAX -D__USE_MINGW_ANSI_STDIO=1 \
                 -DG_STATIC_COMPILATION -DCONFIG_TARGET=\"config-target.h\" \
                 -I$(HERE) -I$(GEN) -I$(GEN)/include -I$(GEN)/trace -I$(GEN)/qapi \
                 -I$(GLIB_BUILD) -I$(GLIB_BUILD)/glib \
                 -I$(GLIB_COPY) -I$(GLIB_COPY)/glib -I$(GLIB_COPY)/gmodule \
                 $(addprefix -I$(ROOT)/,include target/i386 $(LATX) $(LATX)/include \
                 $(LATX)/capstone_git/include $(LATX)/capstone_git/include/capstone \
                 $(LATX)/capstone_git/arch/X86 linux-user linux-user/host/loongarch \
                 linux-user/x86_64 tcg/loongarch) -I$(ROOT)
CORE_CFLAGS := $(PE_CFLAGS) -std=gnu11 -Werror=implicit-function-declaration \
               -Werror=incompatible-pointer-types

.PHONY: wowlatx.dll unix FORCE
wowlatx.dll: $(OUT)/wowlatx.dll
unix: $(OUT)/wowlatx-unix.dll $(OUT)/wowlatx-unix.so

$(OUT)/wowlatx.dll: $(CPU_OBJ) $(CORE_OBJS) $(CAPSTONE_OBJS) $(GLIB_BUILD)/.built $(PE_IMPORTS) $(WINEBUILD)
	$(WINCC) $(PE_CFLAGS) $(PE_LDFLAGS) \
	    -o "$@" $(CPU_OBJ) $(CORE_OBJS) $(CAPSTONE_OBJS) $(GLIB_LIB) $(INTL_LIB) $(PE_IMPORTS) \
	    -lole32 -lshell32 -luuid -ladvapi32 -luser32 -lws2_32
	"$(WINEBUILD)" --builtin "$@"

$(CPU_OBJ): $(ROOT)/$(LATX)/wow64-cpu.c $(CROSS)
	@mkdir -p "$(@D)"
	$(WINCC) $(PE_CFLAGS) $(WINE_INCLUDES) -MMD -MP -c "$<" -o "$@"

$(OUT)/wowlatx-unix.dll: $(ROOT)/$(LATX)/wow64-cpu.c $(HERE)/Makefile $(HERE)/config-host.h $(PE_IMPORTS) $(WINEBUILD)
	@mkdir -p "$(@D)"
	$(WINCC) -fsyntax-only -include "$(HERE)/config-host.h" -x c /dev/null
	$(WINCC) -O2 -ffunction-sections -fdata-sections $(WINE_INCLUDES) -D__WINESRC__ -DLATX_CPU_UNIX \
	    $(PE_LDFLAGS) -nostdlib -Wl,--entry,DllMain -MMD -MP -MF "$@.d" \
	    -o "$@" "$<" $(PE_IMPORTS) -lmsvcrt
	"$(WINEBUILD)" --builtin "$@"

$(OUT)/wowlatx-unix.so: $(ROOT)/$(LATX)/wow64-unixlib.c $(HERE)/Makefile
	@mkdir -p "$(@D)"
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WINE_INCLUDES) -DWINE_UNIX_LIB -fPIC -fvisibility=hidden \
	    -shared $(LDFLAGS) -Wl,-rpath,'$$ORIGIN' -MMD -MP -MF "$@.d" -o "$@" "$<" -ldl -pthread

$(GEN)/ir2/la-append.c: $(GEN)/.built ;

$(CORE_OBJS): $(GEN)/.built $(GLIB_BUILD)/.built $(HERE)/config-host.h $(HERE)/config-target.h
$(CAPSTONE_OBJS): CPPFLAGS += -include $(HERE)/capstone-defs.h
$(CAPSTONE_OBJS): $(HERE)/capstone-defs.h
$(OUT)/obj/$(LATX)/latx-disassemble-trace/capstone_git.o: $(HERE)/capstone-defs.h

$(OUT)/obj/la-append.o: $(GEN)/ir2/la-append.c $(CROSS)
	@mkdir -p "$(@D)"
	$(WINCC) $(CORE_CPPFLAGS) $(CORE_CFLAGS) -MMD -MP -c "$<" -o "$@"

$(OUT)/obj/%.o: $(ROOT)/%.c $(CROSS)
	@mkdir -p "$(@D)"
	$(WINCC) $(CPPFLAGS) $(CORE_CPPFLAGS) $(CORE_CFLAGS) -MMD -MP -c "$<" -o "$@"

HEADER_INPUTS := $(ROOT)/$(LATX)/convert.py $(ROOT)/$(LATX)/inst_template.json \
                 $(ROOT)/scripts/qapi-gen.py $(wildcard $(ROOT)/scripts/qapi/*.py) \
                 $(wildcard $(ROOT)/qapi/*.json) $(ROOT)/scripts/tracetool.py \
                 $(wildcard $(ROOT)/scripts/tracetool/*.py $(ROOT)/scripts/tracetool/*/*.py) \
                 $(addprefix $(ROOT)/,trace-events accel/tcg/trace-events crypto/trace-events \
                 hw/core/trace-events linux-user/trace-events qapi/trace-events qom/trace-events \
                 target/i386/trace-events $(LATX)/trace-events util/trace-events)
$(GEN)/.built: $(HERE)/generate.py $(HEADER_INPUTS)
	$(PYTHON) "$(HERE)/generate.py" "$(GEN)"
	touch "$@"

$(CROSS): FORCE
	@mkdir -p "$(@D)"
	$(PYTHON) "$(HERE)/generate.py" "$@" --compiler "$(WINCC)"

GLIB_FILES := $(shell find "$(GLIB_SRC)" -type f)
$(GLIB_COPY)/.copied: $(GLIB_FILES)
	@mkdir -p "$(GLIB_COPY)"
	cp -R "$(GLIB_SRC)/." "$(GLIB_COPY)/"
	touch "$@"

$(GLIB_BUILD)/.built: $(GLIB_COPY)/.copied $(CROSS) $(HERE)/Makefile
	$(MESON) setup $(if $(wildcard $(GLIB_BUILD)/build.ninja),--wipe) "$(GLIB_BUILD)" "$(GLIB_COPY)" \
	    --cross-file "$(CROSS)" --buildtype=debugoptimized --default-library=static -Dtests=false \
	    -Dnls=disabled -Dsysprof=disabled -Dlibelf=disabled
	$(NINJA) -C "$(GLIB_BUILD)" glib/libglib-2.0.a "$(patsubst $(GLIB_BUILD)/%,%,$(INTL_LIB))"
	touch "$@"

-include $(CPU_OBJ:.o=.d) $(CORE_OBJS:.o=.d) $(CAPSTONE_OBJS:.o=.d)
-include $(OUT)/wowlatx-unix.dll.d $(OUT)/wowlatx-unix.so.d
