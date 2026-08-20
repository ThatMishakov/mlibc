#pragma once
#include <pmos/system.h>
#include <stdint.h>
#include <time.h>

extern uint64_t __process_task_group;

void __pmos_fill_fd_table(void *data_ptr);
pmos_port_t __pmos_prepare_reply_port();

namespace {

int kernel_to_errno(result_t result) {
    return -(int)result;
}

uint64_t timespec_to_kernel(const struct timespec *ts) {
    return ts->tv_sec * (uint64_t)1000000000 + ts->tv_nsec;
}

}