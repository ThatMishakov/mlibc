#pragma once
#include <pmos/system.h>
#include <pmos/memory.h>
#include <stdint.h>
#include <time.h>
#include <sys/mman.h>

namespace mlibc::pmos {

extern uint64_t __process_task_group;
extern pmos_right_t __posix_server_right;

}

pmos_port_t __pmos_prepare_reply_port();

namespace {

int kernel_to_errno(result_t result) {
    return -(int)result;
}

uint64_t timespec_to_kernel(const struct timespec *ts) {
    return ts->tv_sec * (uint64_t)1000000000 + ts->tv_nsec;
}

struct timespec kernel_to_timespec(uint64_t time) {
    struct timespec ts;
    ts.tv_sec = time / 1000000000;
    ts.tv_nsec = time % 1000000000;
    return ts;
}

unsigned mmap_flags_to_kernel(int flags) {
    unsigned result = 0;
    if (flags & MAP_PRIVATE)
        result |= CREATE_FLAG_COW;
    if (flags & MAP_FIXED)
        result |= CREATE_FLAG_FIXED;
    return result;
}

}