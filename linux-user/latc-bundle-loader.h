#ifndef LATC_BUNDLE_LOADER_H
#define LATC_BUNDLE_LOADER_H

#include <stdint.h>

/* Return 1 for a bundle, 0 for a normal runner invocation, or -1 on error. */
int latc_bundle_inject_argv(int *argc, char ***argv);

/* Return the identity of the guest already verified by bundle injection. */
int latc_bundle_verified_guest(uint8_t digest[32], uint64_t *guest_begin,
                               uint64_t *guest_end);
int latc_bundle_verified_guest_digest(uint8_t digest[32]);

struct CPUState;
void latc_bundle_pretranslate(struct CPUState *cpu, uint64_t guest_entry);

/* Called at runtime translator entry and after a TB is installed. */
void latc_bundle_note_tb_attempt(uint64_t guest_pc, uint32_t cflags);
void latc_bundle_note_tb_generated(uint64_t guest_pc, uint32_t cflags);
void latc_bundle_flush_stats(void);

#endif
