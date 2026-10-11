#ifndef LATC_VERIFY_H
#define LATC_VERIFY_H

#include <stddef.h>
#include <stdint.h>

typedef enum LatcVerifyStatus {
    LATC_VERIFY_MATCH = 0,
    LATC_VERIFY_BOUNDARY_MISMATCH,
    LATC_VERIFY_INVALID_INSTRUCTION,
    LATC_VERIFY_BUFFER_TOO_SHORT,
    LATC_VERIFY_TOO_MANY_INSTRUCTIONS,
} LatcVerifyStatus;

typedef struct LatcVerifyResult {
    LatcVerifyStatus status;
    uint64_t start;
    uint64_t expected_end;
    uint64_t actual_end;
    uint32_t instruction_count;
    uint32_t terminator_id;
} LatcVerifyResult;

/* Initialize the decoder selected by the copied LAT configuration. */
void latc_lat_decoder_init(void);

/* Decode through the first LAT TB-ending instruction and compare boundaries. */
LatcVerifyStatus latc_lat_verify_tb(const uint8_t *code, size_t code_size,
                                    uint64_t guest_pc,
                                    uint64_t expected_end,
                                    LatcVerifyResult *result);

#endif
