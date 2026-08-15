#include <mlibc/all-sysdeps.hpp>

#include <pmos/system.h>
#include <pmos/memory.h>
#include <pmos/syscall.h>

#include <stdlib.h>
#include <string.h>

#define STUB()                                                                                     \
    ({                                                                                             \
        __ensure(!"STUB function was called");                                                     \
        __builtin_unreachable();                                                                   \
})

namespace {

int kernel_to_errno(result_t result) {
    return -(int)result;
}

}

namespace mlibc {

int Sysdeps<TcbSet>::operator()(void *ptr) {
    auto result = pmos_set_registers(TASK_ID_SELF, SEGMENT_TCB, ptr);
    return kernel_to_errno(result);
}

int Sysdeps<AnonAllocate>::operator()(size_t size, void **pointer) {
    auto result = create_normal_region(TASK_ID_SELF, NULL, size, 1 | 2 | CREATE_FLAG_COW);
    if (result.result != SUCCESS) {
        return kernel_to_errno(result.result);
    }
    *pointer = result.virt_addr;
    return 0;
}

int Sysdeps<VmUnmap>::operator()(void *ptr, size_t size) {
    auto result = release_memory_range(TASK_ID_SELF, ptr, size);
    return kernel_to_errno(result);
}

int Sysdeps<AnonFree>::operator()(void *ptr, size_t size) {
    return Sysdeps<VmUnmap>()(ptr, size);
}

void Sysdeps<Exit>::operator()(int status) {
    syscall1(SYSCALL_EXIT, (uint64_t)status);
    __builtin_unreachable();
}

void Sysdeps<Yield>::operator()() {
    syscall0(SYSCALL_YIELD);
}

void Sysdeps<LibcLog>::operator()(const char *message) {
    pmos_kernel_debug_log(message, strlen(message));
}

void Sysdeps<LibcPanic>::operator()() {
    auto message = "mlibc panic!!!\n";
    pmos_kernel_debug_log(message, strlen(message));
    Sysdeps<Exit>()(1);
}


int Sysdeps<Close>::operator()(int) {
    STUB();
}

int Sysdeps<Write>::operator()(int , const void *, size_t , ssize_t *) {
    STUB();
}

int Sysdeps<Read>::operator()(int , void *, size_t , ssize_t *) {
    STUB();
}

int Sysdeps<Seek>::operator()(int , off_t , int , off_t *) {
    STUB();
}

int Sysdeps<Open>::operator()(const char *, int, mode_t, int *) {
    STUB();
}

int Sysdeps<Isatty>::operator()(int) {
    STUB();
}

int Sysdeps<Recvfrom>::operator()(int , void *, size_t , int , struct sockaddr *, socklen_t *, ssize_t *) {
    STUB();
}

int Sysdeps<Dup2>::operator()(int , int , int) {
    STUB();
}

int Sysdeps<ClockGet>::operator()(int , time_t *, long *) {
    STUB();
}

int Sysdeps<VmMap>::operator()(void *, size_t , int , int , int , off_t , void **) {
    STUB();
}

int Sysdeps<FutexWake>::operator()(int *, bool) {
    STUB();
}

int Sysdeps<FutexWait>::operator()(int *, int, const struct timespec *) {
    STUB();
}

}