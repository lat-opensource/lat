#ifndef LATC_ENTER_X86_H
#define LATC_ENTER_X86_H

__attribute__((noreturn))
void lat_native_enter_x86_static_exec(void *entry, void *environment,
                                      void *stack_top, void *jump_cache);

#endif
