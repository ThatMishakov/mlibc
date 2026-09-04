#include <stdint.h>
#include <stdlib.h>
#include <mlibc/elf/startup.h>
#include <sys/auxv.h>
#include <mlibc/debug.hpp>
#include <pmos/system.h>
#include <bits/ensure.h>
#include "common.hpp"

extern "C" void __dlapi_enter(uintptr_t *);

extern char **environ;

uint64_t __process_task_group;

pmos_right_t __posix_server_right = INVALID_RIGHT;

namespace {

#if MLIBC_STATIC_BUILD

int auxvec_find(uintptr_t *auxv, unsigned long type, unsigned long &value) {
    while (*auxv != DT_NULL) {
        if (*auxv == type) {
            value = *(auxv + 1);
            return 0;
        }
        auxv += 2;
    }
    return -1;
}

void init_namespace(uintptr_t *auxv) {
    unsigned long value;
    int result = auxvec_find(auxv, AT_TASK_GROUP_ID, value);
    if (result < 0) {
        auto result = create_task_group();
        if (result.result != SUCCESS)
            __ensure(!"Failed to create task group during mlibc init");

        __process_task_group = result.value;
    } else {
        __process_task_group = *(uint64_t *)value;
    }

    auto set_result = set_namespace(__process_task_group, NAMESPACE_RIGHTS);
    if (set_result.result != SUCCESS)
        __ensure(!"Failed to set task group namespace during mlibc init");
}

void init_posix_right(uintptr_t *auxv) {
    unsigned long value;
    int result = auxvec_find(auxv, AT_POSIX_RIGHT, value);
    if (result < 0) {
        __posix_server_right = INVALID_RIGHT;
    } else {
        __posix_server_right = *(uint64_t *)value;
    }
}

void prepare_libc(uintptr_t *entry_stack) {
    // Find auxvector
    auto aux = entry_stack;
    aux += *aux + 1;
    __ensure(!*aux);
    aux++;
    // Skip environment
    while (*aux++) ;

    init_namespace(aux);
    init_posix_right(aux);

    unsigned long value;
    int auxv_result = auxvec_find(aux, AT_FD_TABLE, value);
    if (!auxv_result) {
        __pmos_fill_fd_table((void *)value);
    }
}

#else
void prepare_libc(uintptr_t *) {}
#endif

}

size_t __hwcap;

extern "C" void __mlibc_entry(uintptr_t *entry_stack, int (*main_fn)(int argc, char *argv[], char *env[])) {
    prepare_libc(entry_stack);

    __dlapi_enter(entry_stack);
    __hwcap = getauxval(AT_HWCAP);
    auto result = main_fn(mlibc::entry_stack.argc, mlibc::entry_stack.argv, environ);
    exit(result);
}