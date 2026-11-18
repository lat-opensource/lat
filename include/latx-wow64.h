/* SPDX-License-Identifier: GPL-2.0-or-later */
/* i386 execution interface for Wine WoW64; independent of any EC ABI. */
#ifndef LAT_WOW64_PROVIDER_H
#define LAT_WOW64_PROVIDER_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LAT_WOW64_ABI_VERSION 1u
#define LAT_WOW64_MAX_ENTER_DEPTH 64u

/* Optional Wine host hook. Post completes a matching pre-termination call. */
void lat_wow64_process_term(uint32_t post);

typedef struct LatWow64Process LatWow64Process;
typedef struct LatWow64Thread LatWow64Thread;

typedef enum LatWow64Status {
    LAT_WOW64_STATUS_OK = 0,
    LAT_WOW64_STATUS_INVALID_ARGUMENT = -1,
    LAT_WOW64_STATUS_VERSION_MISMATCH = -2,
    LAT_WOW64_STATUS_NO_MEMORY = -3,
    LAT_WOW64_STATUS_BUSY = -4,
    LAT_WOW64_STATUS_NOT_FOUND = -5,
    LAT_WOW64_STATUS_OVERLAP = -6,
    LAT_WOW64_STATUS_STALE_MAPPING = -7,
    LAT_WOW64_STATUS_UNROUTED_SYSCALL = -8,
    LAT_WOW64_STATUS_BACKEND_ERROR = -9,
    LAT_WOW64_STATUS_INVALID_STATE = -10,
    LAT_WOW64_STATUS_PROTOCOL_ERROR = -11,
} LatWow64Status;

typedef enum LatWow64ExitReason {
    LAT_WOW64_EXIT_NONE = 0,
    LAT_WOW64_EXIT_RETURN,
    LAT_WOW64_EXIT_CALLBACK,
    LAT_WOW64_EXIT_EXCEPTION,
    LAT_WOW64_EXIT_BREAKPOINT,
    LAT_WOW64_EXIT_SYSCALL,
    LAT_WOW64_EXIT_SYSCALL_UNROUTED,
    LAT_WOW64_EXIT_STALE_MAPPING,
    LAT_WOW64_EXIT_STOPPED,
    LAT_WOW64_EXIT_SINGLE_STEP,
} LatWow64ExitReason;

typedef enum LatWow64ReplyAction {
    LAT_WOW64_REPLY_NONE = 0,
    LAT_WOW64_REPLY_CONTINUE,
    LAT_WOW64_REPLY_NOT_HANDLED,
    LAT_WOW64_REPLY_STOP,
    LAT_WOW64_REPLY_RETRY,
    LAT_WOW64_REPLY_REDIRECT,
} LatWow64ReplyAction;

typedef enum LatWow64ThreadState {
    LAT_WOW64_THREAD_READY = 1,
    LAT_WOW64_THREAD_RUNNING,
    LAT_WOW64_THREAD_WAITING_REPLY,
    LAT_WOW64_THREAD_SUSPENDED,
    LAT_WOW64_THREAD_STOPPED,
    LAT_WOW64_THREAD_DESTROYING,
} LatWow64ThreadState;

enum {
    LAT_WOW64_EXIT_FLAG_SAFE_POINT = 1u << 0,
    LAT_WOW64_EXIT_FLAG_REPLY_REQUIRED = 1u << 1,
    LAT_WOW64_EXIT_FLAG_TRAP_NUMBER_VALID = 1u << 2,
    LAT_WOW64_EXIT_FLAG_ERROR_CODE_VALID = 1u << 3,
    LAT_WOW64_EXIT_FLAG_FAULT_ADDRESS_VALID = 1u << 4,
    LAT_WOW64_EXIT_FLAG_SERVICE_NUMBER_VALID = 1u << 5,
};

#define LAT_WOW64_REPLY_MASK(action) (1u << (action))
#define LAT_WOW64_ENTER_CURRENT_PC UINT64_MAX

typedef enum LatWow64MemoryEventType {
    LAT_WOW64_MEMORY_MAP = 1,
    LAT_WOW64_MEMORY_PROTECT,
    LAT_WOW64_MEMORY_UNMAP,
    LAT_WOW64_MEMORY_WRITE,
    LAT_WOW64_MEMORY_FLUSH,
} LatWow64MemoryEventType;

enum {
    LAT_WOW64_PROT_READ = 1u << 0,
    LAT_WOW64_PROT_WRITE = 1u << 1,
    LAT_WOW64_PROT_EXEC = 1u << 2,
};

typedef enum LatWow64Register {
    LAT_WOW64_EAX,
    LAT_WOW64_ECX,
    LAT_WOW64_EDX,
    LAT_WOW64_EBX,
    LAT_WOW64_ESP,
    LAT_WOW64_EBP,
    LAT_WOW64_ESI,
    LAT_WOW64_EDI,
    LAT_WOW64_REGISTER_COUNT,
} LatWow64Register;

typedef struct LatWow64Context {
    uint32_t size;
    uint32_t abi_version;
    uint32_t gpr[LAT_WOW64_REGISTER_COUNT];
    uint32_t eip;
    uint32_t eflags;
    uint16_t cs;
    uint16_t ss;
    uint16_t ds;
    uint16_t es;
    uint16_t fs;
    uint16_t gs;
    uint32_t fs_base;
    uint32_t gs_base;
    uint32_t dr0;
    uint32_t dr1;
    uint32_t dr2;
    uint32_t dr3;
    uint32_t dr6;
    /* Enabled hardware breakpoints (DR7 bits 0..7) are not supported.
     * thread_create and set_context reject them with INVALID_ARGUMENT. */
    uint32_t dr7;
    uint8_t fxsave[512];
    uint64_t generation;
} LatWow64Context;

typedef struct LatWow64Exit {
    uint32_t size;
    uint32_t abi_version;
    uint32_t reason;
    uint32_t flags;
    uint64_t guest_pc;
    uint64_t detail;
    uint64_t mapping_generation;
    uint64_t sequence;
    uint32_t reply_mask;
    uint32_t reserved;
    uint32_t trap_number;
    uint32_t error_code;
    uint64_t fault_address;
    uint64_t service_number;
    uint64_t context_generation;
} LatWow64Exit;

typedef struct LatWow64Reply {
    uint32_t size;
    uint32_t abi_version;
    uint64_t sequence;
    uint32_t action;
    uint32_t flags;
    uint64_t detail;
} LatWow64Reply;

typedef struct LatWow64ThreadInfo {
    uint32_t size;
    uint32_t abi_version;
    uint32_t state;
    uint32_t enter_depth;
    uint64_t context_generation;
    uint64_t pending_sequence;
} LatWow64ThreadInfo;

typedef struct LatWow64ProcessConfig {
    uint32_t size;
    uint32_t abi_version;
    uint32_t max_enter_depth;
    uint32_t flags;
    uint64_t return_address;
} LatWow64ProcessConfig;

typedef struct LatWow64MemoryEvent {
    uint32_t size;
    uint32_t abi_version;
    uint32_t type;
    uint32_t protection;
    uint64_t address;
    uint64_t length;
} LatWow64MemoryEvent;

typedef struct LatWow64Api {
    uint32_t size;
    uint32_t abi_version;
    LatWow64Status (*process_create)(const LatWow64ProcessConfig *, LatWow64Process **);
    LatWow64Status (*process_destroy)(LatWow64Process *);
    LatWow64Status (*thread_create)(LatWow64Process *, const LatWow64Context *, LatWow64Thread **);
    LatWow64Status (*thread_destroy)(LatWow64Thread *);
    LatWow64Status (*get_context)(LatWow64Thread *, LatWow64Context *);
    LatWow64Status (*set_context)(LatWow64Thread *, const LatWow64Context *);
    LatWow64Status (*enter)(LatWow64Thread *, uint64_t, LatWow64Exit *);
    LatWow64Status (*reply)(LatWow64Thread *, const LatWow64Reply *);
    LatWow64Status (*memory_event)(LatWow64Process *, const LatWow64MemoryEvent *);
    LatWow64Status (*thread_get_info)(LatWow64Thread *, LatWow64ThreadInfo *);
    LatWow64Status (*thread_suspend)(LatWow64Thread *);
    LatWow64Status (*thread_resume)(LatWow64Thread *);
    /* Async-signal-safe. Only the host thread currently using this handle
     * may request its own interruption; the handle must remain alive. Pass the
     * original host signal ucontext, or NULL outside a signal handler. */
    void (*request_interrupt)(LatWow64Thread *, void *host_signal_context);
} LatWow64Api;

/* Only this function is exported by liblatx-wow64.so. */
const LatWow64Api *lat_wow64_get_api(uint32_t abi_version);

#ifdef __cplusplus
}
#endif
#endif
