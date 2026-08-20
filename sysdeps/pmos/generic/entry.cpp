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

void init_namespace() {
    unsigned long value;
    int result = peekauxval(AT_TASK_GROUP_ID, &value);
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

void init_posix_right() {
    unsigned long value;
    int result = peekauxval(AT_POSIX_RIGHT, &value);
    if (result < 0) {
        __posix_server_right = INVALID_RIGHT;
    } else {
        __posix_server_right = *(uint64_t *)value;
    }
}

}

extern "C" void __mlibc_entry(uintptr_t *entry_stack, int (*main_fn)(int argc, char *argv[], char *env[])) {
    __dlapi_enter(entry_stack);

    init_namespace();
    init_posix_right();

    unsigned long value;
    int auxv_result = peekauxval(AT_FD_TABLE, &value);
    if (!auxv_result) {
        __pmos_fill_fd_table((void *)value);
    }

    auto result = main_fn(mlibc::entry_stack.argc, mlibc::entry_stack.argv, environ);
    exit(result);
}