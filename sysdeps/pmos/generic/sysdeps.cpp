#include <mlibc/all-sysdeps.hpp>

#include <pmos/system.h>
#include <pmos/memory.h>
#include <pmos/syscall.h>
#include "common.hpp"

#include <stdlib.h>
#include <string.h>

#include <mlibc/debug.hpp>
#include <mlibc/tcb.hpp>

#define STUB()                                                                                     \
    ({                                                                                             \
        __ensure(!"STUB function was called");                                                     \
        __builtin_unreachable();                                                                   \
})

namespace mlibc {

int Sysdeps<TcbSet>::operator()(void *ptr) {
    #if defined(__x86_64__) || defined(__i386__)
    auto tcb_ptr = ptr;
    #else
    auto tcb_ptr = reinterpret_cast<void *>(reinterpret_cast<uintptr_t>(ptr) + sizeof(Tcb));
    #endif

    auto result = pmos_set_registers(TASK_ID_SELF, SEGMENT_TCB, tcb_ptr);
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

[[noreturn]] void Sysdeps<Exit>::operator()(int status) {
    syscall2(SYSCALL_EXIT, (uint64_t)status, true);
    __builtin_unreachable();
}

void Sysdeps<Yield>::operator()() {
    syscall0(SYSCALL_YIELD);
}

void Sysdeps<LibcLog>::operator()(const char *message) {
    pmos_kernel_debug_log(message, strlen(message));
    pmos_kernel_debug_log("\n", 1);
}

void Sysdeps<LibcPanic>::operator()() {
    auto message = "mlibc panic!!!\n";
    pmos_kernel_debug_log(message, strlen(message));
    Sysdeps<Exit>()(1);
}



int Sysdeps<ClockGet>::operator()(int , time_t *, long *) {
    STUB();
}

int Sysdeps<FutexWake>::operator()(int *pointer, bool all) {
    auto result = pmos_futex_wake(pointer, all);
    return kernel_to_errno(result);
}

int Sysdeps<FutexWait>::operator()(int *pointer, int expected, const struct timespec *timeout) {
    uint64_t timeout_ns = timeout ? timespec_to_kernel(timeout) : -1;
    auto result = pmos_futex_wait(pointer, expected, timeout_ns);
    return kernel_to_errno(result);
}

}