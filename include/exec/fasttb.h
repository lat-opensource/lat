#ifndef FASTTB_H
#define FASTTB_H
struct FastTB {
    unsigned long pc;
    const void *ptr;    /* pointer to the translated code */
};

void latx_fast_jmp_cache_add(CPUState *cs, int h, struct TranslationBlock *tb);
void latx_fast_jmp_cache_clear(CPUState *cs, int h);
void latx_fast_jmp_cache_clear_all(CPUState *cs);
bool latx_fast_jmp_cache_init(void *env);
#endif
