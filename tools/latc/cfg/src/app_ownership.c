#include "app_ownership.h"

#include <inttypes.h>
#include <string.h>

static bool starts_with(const char *s, const char *prefix)
{
    return strncmp(s, prefix, strlen(prefix)) == 0;
}

static bool exact(const char *s, const char *name)
{
    return strcmp(s, name) == 0;
}

static bool known_runtime_or_library_name(const char *name)
{
    if (strstr(name, "_MOD_")) {
        return false;
    }

    static const char *const prefixes[] = {
        "__",
        "_gfortran",
        "_gfortrani",
        "_IO_",
        "_Unwind_",
        "_dl_",
        "_nl_",
        "pthread_",
        NULL,
    };
    static const char *const exact_names[] = {
        "_DYNAMIC",
        "_GLOBAL_OFFSET_TABLE_",
        "_dl_relocate_static_pie",
        "_fini",
        "_init",
        "_start",
        "abort",
        "calloc",
        "call_fini",
        "cleanup",
        "constructor_recursion_check",
        "deregister_tm_clones",
        "destructor_recursion_check",
        "elf_zlib_inflate.cold",
        "exit",
        "fclose",
        "fflush",
        "fgets",
        "fopen",
        "fprintf",
        "frame_dummy",
        "free",
        "fseek",
        "fwrite",
        "get_common_cache_info.constprop.0",
        "get_common_indices.constprop.0",
        "getenv",
        "gsignal",
        "get_available_features.constprop.0",
        "handle_amd",
        "handle_intel.constprop.0",
        "handle_zhaoxin",
        "has_cpu_feature.part.0.constprop.0",
        "intel_check_word.constprop.0",
        "init",
        "malloc",
        "memchr",
        "memcmp",
        "memcpy",
        "memmove",
        "mempcpy",
        "memrchr",
        "memset",
        "printf",
        "putchar",
        "puts",
        "qsort",
        "raise",
        "realloc",
        "register_printf_flt128",
        "register_tm_clones",
        "remove",
        "set_fast_math",
        "sigaction",
        "signal",
        "snprintf",
        "sprintf",
        "stpcpy",
        "stpncpy",
        "strcasecmp",
        "strcat",
        "strchr",
        "strchrnul",
        "strcmp",
        "strcpy",
        "strcspn",
        "strdup",
        "strerror",
        "strlen",
        "strncasecmp",
        "strncat",
        "strncmp",
        "strncpy",
        "strnlen",
        "strpbrk",
        "strrchr",
        "strsep",
        "strspn",
        "strstr",
        "strtok",
        "strtol",
        "strtoul",
        "tdestroy",
        "unregister_printf_flt128",
        "update_active.constprop.0",
        NULL,
    };

    for (size_t i = 0; prefixes[i]; i++) {
        if (starts_with(name, prefixes[i])) {
            return true;
        }
    }
    for (size_t i = 0; exact_names[i]; i++) {
        if (exact(name, exact_names[i])) {
            return true;
        }
    }

    /*
     * Common static libc/compiler helpers. These are intentionally broad; the
     * app summary is a convenience filter and is allowed to have false
     * positives/negatives.
     */
    return starts_with(name, "_quicksort") ||
           starts_with(name, "msort_with_tmp");
}

void app_ownership_detect(const FuncVec *funcs, AppOwnership *owner)
{
    memset(owner, 0, sizeof(*owner));
    owner->method = "main-contiguous-known-lib-stop";

    size_t main_idx = funcs->n;
    for (size_t i = 0; i < funcs->n; i++) {
        if (exact(funcs->v[i].name, "main")) {
            main_idx = i;
            break;
        }
    }
    if (main_idx == funcs->n) {
        owner->reason = "main-symbol-not-found";
        return;
    }

    owner->available = true;
    owner->start = funcs->v[main_idx].addr;
    owner->end = funcs->v[main_idx].addr + funcs->v[main_idx].size;
    owner->reason = "main-symbol";

    bool saw_app_after_main = false;
    for (size_t i = main_idx + 1; i < funcs->n; i++) {
        const FuncSym *fn = &funcs->v[i];
        bool is_lib = known_runtime_or_library_name(fn->name);

        if (!saw_app_after_main && is_lib) {
            continue;
        }
        if (saw_app_after_main && is_lib) {
            owner->end = fn->addr;
            break;
        }

        saw_app_after_main = true;
        owner->end = fn->addr + fn->size;
    }

    for (size_t i = 0; i < funcs->n; i++) {
        if (app_ownership_contains(owner, &funcs->v[i])) {
            owner->functions++;
        }
    }
}

bool app_ownership_contains(const AppOwnership *owner, const FuncSym *fn)
{
    if (!owner->available) {
        return false;
    }
    if (fn->addr < owner->start || fn->addr >= owner->end) {
        return false;
    }
    return !known_runtime_or_library_name(fn->name);
}

void app_ownership_print_filter_summary(FILE *out,
                                        const AppOwnership *owner)
{
    fprintf(out, "app_filter_summary\n");
    fprintf(out, "  available=%s\n", owner->available ? "yes" : "no");
    fprintf(out, "  method=%s\n", owner->method);
    fprintf(out, "  reason=%s\n", owner->reason);
    if (owner->available) {
        fprintf(out, "  range=0x%016" PRIx64 "-0x%016" PRIx64 "\n",
                owner->start, owner->end);
        fprintf(out, "  functions=%zu\n", owner->functions);
        fprintf(out, "  precision=heuristic false_positives_allowed\n");
    }
    putc('\n', out);
}

void app_ownership_print_result_summary(FILE *out,
                                        const AppOwnership *owner,
                                        const CfgCheckSummary *check,
                                        const CfgGraphSummary *graph)
{
    if (!owner->available) {
        fprintf(out, "app_cfg_check_summary\n");
        fprintf(out, "  available=no\n");
        fprintf(out, "  reason=%s\n", owner->reason);
        return;
    }
    cfg_check_print_summary_named(out, "app_cfg_check_summary", check);
    cfg_graph_print_summary(out, "app_transfer_summary", graph);
}
