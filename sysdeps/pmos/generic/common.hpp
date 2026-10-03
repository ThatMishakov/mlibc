#pragma once
#include <pmos/system.h>
#include <pmos/memory.h>
#include <stdint.h>
#include <time.h>
#include <sys/mman.h>
#include <pmos/ipc.h>
#include <limits.h>
#include <mlibc/threads.hpp>
#include <frg/array.hpp>
#include <mlibc/lock.hpp>

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

unsigned flags_to_io(unsigned fd_flags) {
    unsigned io_flags = 0;
    if (fd_flags & O_APPEND)
        io_flags |= IPC_FLAG_IO_OP_APPEND;
    if (fd_flags & O_NONBLOCK)
        io_flags |= IPC_FLAG_IO_OP_NONBLOCK;
    return io_flags;
}

}

namespace mlibc::pmos {

struct RightWrapper {
    pmos_right_t right = INVALID_RIGHT;
    ~RightWrapper() {
        if (right != INVALID_RIGHT)
            delete_right(right);
    }

    RightWrapper() = default;
    RightWrapper(pmos_right_t r) : right(r) {}
    RightWrapper(RightWrapper &&other) : right(other.right) {
        other.right = INVALID_RIGHT;
    }
    RightWrapper &operator=(RightWrapper &&other) {
        if (this != &other) {
            if (right != INVALID_RIGHT)
                delete_right(right);
            right = other.right;
            other.right = INVALID_RIGHT;
        }
        return *this;
    }
    RightWrapper(const RightWrapper &) = delete;
    RightWrapper &operator=(const RightWrapper &) = delete;
};

struct OpenFile {
    pmos_right_t io_right;
    pmos_right_t op_right;
    unsigned flags;
};

extern FutexLock filesystem_mutex;
extern frg::array<OpenFile, __MLIBC_OPEN_MAX> open_files;

} // namespace mlibc::pmos