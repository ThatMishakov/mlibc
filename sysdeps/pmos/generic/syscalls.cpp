#include <kernel/syscalls.h>
#include <pmos/syscall.h>
#include <pmos/system.h>
#include <pmos/memory.h>

// TODO: This should probably be a separate library

result_t release_memory_range(uint64_t task_id, void *start, size_t size)
{
#ifdef __32BITSYSCALL
    return __pmos_syscall32_4words(SYSCALL_UNMAP_RANGE, task_id, (unsigned)start, size).result;
#else
    return syscall3(SYSCALL_UNMAP_RANGE, (uint64_t)task_id, (uint64_t)start, (uint64_t)size).result;
#endif
}

result_t pmos_set_registers(uint64_t pid, unsigned segment, void *addr)
{
#ifdef __32BITSYSCALL
    return __pmos_syscall32_4words(SYSCALL_SET_REGISTERS, pid, segment, (unsigned)addr).result;
#else
    return syscall3(SYSCALL_SET_REGISTERS, (uint64_t)pid, (uint64_t)segment, (uint64_t)addr).result;
#endif
}

mem_request_ret_t create_normal_region(uint64_t pid, void *addr_start, size_t size, uint32_t access)
{
#ifdef __32BITSYSCALL
    syscall_r r = __pmos_syscall32_4words(SYSCALL_CREATE_NORMAL_REGION | (access << 8), pid,
                                          addr_start, size);
#else
    syscall_r r = syscall3(SYSCALL_CREATE_NORMAL_REGION | (access << 8), (uint64_t)pid, (uint64_t)addr_start, (uint64_t)size);
#endif
    mem_request_ret_t t = {
        .result = static_cast<result_t>(r.result),
        .virt_addr_intptr = r.value
    };
    return t;
}

result_t pmos_kernel_debug_log(const char *string, size_t length)
{
    return syscall2(SYSCALL_DEBUG_LOG, (intptr_t)string, length).result;
}