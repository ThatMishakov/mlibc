#include <kernel/syscalls.h>
#include <pmos/syscall.h>
#include <pmos/system.h>
#include <pmos/memory.h>
#include <pmos/ports.h>
#include <errno.h>

// TODO: This should probably be a separate library

extern "C" {

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

uint64_t get_task_id()
{
    return syscall0(SYSCALL_GET_TASK_ID).value;
}

ports_request_t create_port(uint64_t owner, uint32_t flags)
{
#ifdef __32BITSYSCALL
    auto r = __pmos_syscall32_2words(SYSCALL_CREATE_PORT | (flags << 8), owner);
#else
    auto r = syscall1(SYSCALL_CREATE_PORT | (flags << 8), owner);
#endif

    ports_request_t t = {static_cast<result_t>(r.result), r.value};
    return t;
}

right_request_t create_right(uint64_t port_id, pmos_right_t *id_in_reciever, unsigned flags)
{
    syscall_r result;
#ifdef __32BITSYSCALL
    result = __pmos_syscall32_3words(SYSCALL_CREATE_RIGHT | (flags << 8), port_id, id_in_reciever);
#else
    result = syscall2(SYSCALL_CREATE_RIGHT | (flags << 8), port_id, reinterpret_cast<uint64_t>(id_in_reciever));
#endif
    return (right_request_t) {
        .result = static_cast<result_t>(result.result),
        .right = result.value,
    };
}

result_t delete_right(pmos_right_t right_id)
{
    if (!right_id)
        return EINVAL;

    #ifdef __32BITSYSCALL
    return __pmos_syscall32_2words(SYSCALL_DELETE_SEND_RIGHT, right_id).result;
    #else
    return syscall1(SYSCALL_DELETE_SEND_RIGHT, right_id).result;
    #endif
}

result_t pmos_delete_port(pmos_port_t port)
{
#ifdef __32BITSYSCALL
    return __pmos_syscall32_2words(SYSCALL_DELETE_PORT, port).result;
#else
    return syscall1(SYSCALL_DELETE_PORT, port).result;
#endif
}

right_request_t watch_right(pmos_right_t right, pmos_port_t port)
{
    syscall_r result;
    #ifdef __32BITSYSCALL
    result = __pmos_syscall32_4words(SYSCALL_WATCH_RIGHT, right, port);
    #else
    result = syscall2(SYSCALL_WATCH_RIGHT, right, port);
    #endif
    return (right_request_t) {
        .result = static_cast<result_t>(result.result),
        .right = result.value,
    };
}

result_t accept_rights(pmos_port_t port, pmos_right_t *rights_array)
{
    #ifdef __32BITSYSCALL
    return __pmos_syscall32_3words(SYSCALL_ACCEPT_RIGHTS, port, rights_array).result;
    #else
    return syscall2(SYSCALL_ACCEPT_RIGHTS, port, reinterpret_cast<uintptr_t>(rights_array)).result;
    #endif
}

right_request_t get_first_message(char *buff, uint32_t args, uint64_t port)
{
    syscall_r result;
#ifdef __32BITSYSCALL
    result = __pmos_syscall32_3words(SYSCALL_GET_MESSAGE | (args << 8), port, (unsigned)buff);
#else
    result = syscall2(SYSCALL_GET_MESSAGE | (args << 8), port, reinterpret_cast<uintptr_t>(buff));
#endif
    return (right_request_t) {
        .result = static_cast<result_t>(result.result),
        .right = result.value,
    };
}

result_t syscall_get_message_info(Message_Descriptor *descr, uint64_t port, uint32_t flags)
{
#ifdef __32BITSYSCALL
    return __pmos_syscall32_3words(SYSCALL_GET_MSG_INFO | (flags << 8), port, (unsigned)descr)
        .result;
#else
    return syscall2(SYSCALL_GET_MSG_INFO | (flags << 8), port, reinterpret_cast<uintptr_t>(descr)).result;
#endif
}


} // extern "C"