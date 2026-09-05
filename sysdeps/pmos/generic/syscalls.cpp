#include <kernel/syscalls.h>
#include <pmos/syscall.h>
#include <pmos/system.h>
#include <pmos/memory.h>
#include <pmos/ports.h>
#include <pmos/interrupts.h>
#include <errno.h>

// TODO: This should probably be a separate library

#ifdef __i386__
#define __32BITSYSCALL
#endif

extern "C" {

result_t release_memory_range(uint64_t task_id, void *start, size_t size)
{
#ifdef __32BITSYSCALL
    return syscall32_4(SYSCALL_UNMAP_RANGE, (uint32_t)task_id, task_id >> 32, (unsigned)start, size).result;
#else
    return syscall3(SYSCALL_UNMAP_RANGE, (uint64_t)task_id, (uint64_t)start, (uint64_t)size).result;
#endif
}

result_t pmos_set_registers(uint64_t pid, unsigned segment, void *addr)
{
#ifdef __32BITSYSCALL
    return syscall32_4(SYSCALL_SET_REGISTERS, (uint32_t)pid, pid >> 32, segment, (unsigned)addr).result;
#else
    return syscall3(SYSCALL_SET_REGISTERS, (uint64_t)pid, (uint64_t)segment, (uint64_t)addr).result;
#endif
}

mem_request_ret_t create_normal_region(uint64_t pid, void *addr_start, size_t size, uint32_t access)
{
#ifdef __32BITSYSCALL
    syscall_r r = syscall32_4(SYSCALL_CREATE_NORMAL_REGION | (access << 8), (uint32_t)pid, pid >> 32, (unsigned)addr_start, size);
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
#ifdef __32BITSYSCALL
    return syscall32_2(SYSCALL_DEBUG_LOG, (unsigned)string, length).result;
#else
    return syscall2(SYSCALL_DEBUG_LOG, (intptr_t)string, length).result;
#endif
}

uint64_t get_task_id()
{
#ifdef __32BITSYSCALL
    return syscall32_0(SYSCALL_GET_TASK_ID).value;
#else
    return syscall0(SYSCALL_GET_TASK_ID).value;
#endif
}

ports_request_t create_port(uint64_t owner, uint32_t flags)
{
#ifdef __32BITSYSCALL
    auto r = syscall32_2(SYSCALL_CREATE_PORT | (flags << 8), (uint32_t)owner, owner >> 32);
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
    result = syscall32_3(SYSCALL_CREATE_RIGHT | (flags << 8), (uint32_t)port_id, port_id >> 32, (uintptr_t)id_in_reciever);
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
    return syscall32_2(SYSCALL_DELETE_SEND_RIGHT, (uint32_t)right_id, right_id >> 32).result;
    #else
    return syscall1(SYSCALL_DELETE_SEND_RIGHT, right_id).result;
    #endif
}

result_t pmos_delete_port(pmos_port_t port)
{
#ifdef __32BITSYSCALL
    return syscall32_2(SYSCALL_DELETE_PORT, (uint32_t)port, port >> 32).result;
#else
    return syscall1(SYSCALL_DELETE_PORT, port).result;
#endif
}

right_request_t watch_right(pmos_right_t right, pmos_port_t port)
{
    syscall_r result;
    #ifdef __32BITSYSCALL
    result = syscall32_4(SYSCALL_WATCH_RIGHT, (uint32_t)right, right >> 32, (uint32_t)port, port >> 32);
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
    return syscall32_3(SYSCALL_ACCEPT_RIGHTS, (uint32_t)port, port >> 32, (uintptr_t)rights_array).result;
    #else
    return syscall2(SYSCALL_ACCEPT_RIGHTS, port, reinterpret_cast<uintptr_t>(rights_array)).result;
    #endif
}

right_request_t get_first_message(char *buff, uint32_t args, uint64_t port)
{
    syscall_r result;
#ifdef __32BITSYSCALL
    result = syscall32_3(SYSCALL_GET_MESSAGE | (args << 8), (uint32_t)port, port >> 32, (unsigned)buff);
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
    return syscall32_3(SYSCALL_GET_MSG_INFO | (flags << 8), (uint32_t)port, port >> 32, (unsigned)descr)
        .result;
#else
    return syscall2(SYSCALL_GET_MSG_INFO | (flags << 8), port, reinterpret_cast<uintptr_t>(descr)).result;
#endif
}

right_request_t send_message_right(pmos_right_t send_right, pmos_port_t reply_port, const void *message,
                             size_t message_size, message_extra_t *aux_stuff, unsigned flags)
{
    syscall_r result;
#ifdef __32BITSYSCALL
    result = syscall32_7(SYSCALL_SEND_MSG_RIGHT | (flags << 8), (uint32_t)send_right, send_right >> 32, (uint32_t)reply_port, reply_port >> 32, (unsigned)
                                   (uintptr_t)message, message_size, (uintptr_t)aux_stuff);
#else
    result = syscall5(SYSCALL_SEND_MSG_RIGHT | (flags << 8), send_right, reply_port, reinterpret_cast<uintptr_t>(message),
                        message_size, reinterpret_cast<uintptr_t>(aux_stuff));
#endif
    return (right_request_t) {
        .result = static_cast<result_t>(result.result),
        .right = result.value,
    };
}

result_t set_affinity(uint64_t tid, uint32_t cpu_id, unsigned flags)
{
#ifdef __32BITSYSCALL
    return syscall32_3(SYSCALL_SET_AFFINITY | (flags << 8), (uint32_t)tid, tid >> 32, cpu_id).result;
#else
    return syscall2(SYSCALL_SET_AFFINITY | (flags << 8), tid, cpu_id).result;
#endif
}

right_request_t allocate_interrupt(uint32_t gsi, uint32_t flags)
{
    syscall_r result;
#ifdef __32BITSYSCALL
    result = syscall32_1(SYSCALL_ALLOCATE_INTERRUPT | (flags << 8), gsi);
#else
    result = syscall1(SYSCALL_ALLOCATE_INTERRUPT | (flags << 8), gsi);
#endif
    return (right_request_t) {
        .result = static_cast<result_t>(result.result),
        .right = result.value,
    };
}

right_request_t dup_right(pmos_right_t right)
{
    syscall_r result;
    #ifdef __32BITSYSCALL
    result = syscall32_2(SYSCALL_DUP_RIGHT, (uint32_t)right, right >> 32);
    #else
    result = syscall1(SYSCALL_DUP_RIGHT, right);
    #endif
    return (right_request_t) {
        .result = static_cast<result_t>(result.result),
        .right = result.value,
    };
}

mem_request_ret_t map_mem_object(const map_mem_object_param_t *params)
{
#ifdef __32BITSYSCALL
    syscall_r r = syscall32_1(SYSCALL_MAP_MEM_OBJECT, reinterpret_cast<uintptr_t>(params));
#else
    syscall_r r = syscall1(SYSCALL_MAP_MEM_OBJECT, reinterpret_cast<uintptr_t>(params));
#endif
    mem_request_ret_t t = {
        .result = static_cast<result_t>(r.result),
        .virt_addr_intptr = r.value
    };
    return t;
}

result_t complete_interrupt(pmos_port_t port, pmos_right_t receive_right)
{
#ifdef __32BITSYSCALL
    return syscall32_4(SYSCALL_COMPLETE_INTERRUPT, (uint32_t)port, port >> 32, (uint32_t)receive_right, receive_right >> 32).result;
#else
    return syscall2(SYSCALL_COMPLETE_INTERRUPT, port, receive_right).result;
#endif
}

interrupt_info_t get_interrupt_affinity(pmos_right_t right)
{
    syscall_r result;
    #ifdef __32BITSYSCALL
    result = syscall32_2(SYSCALL_GET_INTERRUPT_INFO, (uint32_t)right, right >> 32);
    #else
    result = syscall1(SYSCALL_GET_INTERRUPT_INFO, right);
    #endif
    return (interrupt_info_t) {
        .result = static_cast<result_t>(result.result),
        .interrupt_affinity_cpu = static_cast<u32>(result.value & 0xffffffff),
        .arch_vector = static_cast<u32>(result.value >> 32),
    };
}

right_request_t set_interrupt(pmos_right_t right, pmos_port_t port)
{
    syscall_r r;
#ifdef __32BITSYSCALL
    r = syscall32_4(SYSCALL_SET_INTERRUPT, (uint32_t)right, right >> 32, (uint32_t)port, port >> 32);
#else
    r = syscall2(SYSCALL_SET_INTERRUPT, right, port);
#endif
    right_request_t ret = {
        .result = static_cast<result_t>(r.result),
        .right = r.value,
    };
    return ret;
}

mem_request_ret_t create_phys_map_region(uint64_t pid, void *addr_start, size_t size,
                                         uint32_t access, uint64_t phys_addr)
{
#ifdef __32BITSYSCALL
    syscall_r r = syscall32_6(SYSCALL_CREATE_PHYS_REGION | (access << 8), (uint32_t)pid, pid >> 32,
                                          (uint32_t)phys_addr, phys_addr >> 32, reinterpret_cast<uintptr_t>(addr_start), size);
#else
    syscall_r r =
        syscall4(SYSCALL_CREATE_PHYS_REGION | (access << 8), pid, reinterpret_cast<uintptr_t>(phys_addr), reinterpret_cast<uintptr_t>(addr_start), size);
#endif
    mem_request_ret_t t = {
        .result = static_cast<result_t>(r.result),
        .virt_addr_intptr = r.value
    };
    return t;
}

syscall_r pmos_get_time(unsigned mode)
{
#ifdef __32BITSYSCALL
    return syscall32_1(SYSCALL_GET_TIME, mode);
#else
    return syscall1(SYSCALL_GET_TIME, mode);
#endif
}

result_t send_message_port(uint64_t port, size_t size, const void *message)
{
#ifdef __32BITSYSCALL
    return syscall32_4(SYSCALL_SEND_MSG_PORT, (uint32_t)port, port >> 32, (uint32_t)size, (unsigned)message).result;
#else
    return syscall3(SYSCALL_SEND_MSG_PORT, port, size, reinterpret_cast<uintptr_t>(message)).result;
#endif
}

result_t delete_receive_right(pmos_port_t port, pmos_right_t right)
{
    if (!port || !right)
        return SUCCESS;

    #ifdef __i386__
    return syscall32_4(SYSCALL_DELETE_RECEIVE_RIGHT, (uint32_t)port, port >> 32, (uint32_t)right, right >> 32).result;
    #else
    return syscall2(SYSCALL_DELETE_RECEIVE_RIGHT, port, right).result;
    #endif
}

syscall_r __pmos_syscall_set_attr(uint64_t pid, uint32_t attr, unsigned long value)
{
#ifdef __32BITSYSCALL
    return  syscall32_4(SYSCALL_SET_ATTR, (uint32_t)pid, pid >> 32, attr, value);
#else
    return syscall3(SYSCALL_SET_ATTR, pid, attr, value);
#endif
}

result_t set_right0(pmos_right_t right)
{
    syscall_r result;
#ifdef __32BITSYSCALL
    result = syscall32_2(SYSCALL_SET_RIGHT0, (uint32_t)right, right >> 32);
#else
    result = syscall1(SYSCALL_SET_RIGHT0, right);
#endif
    return result.result;
}

right_request_t transfer_right(uint64_t task_group, uint64_t right, unsigned flags)
{
    syscall_r result;
    #ifdef __32BITSYSCALL
    result = syscall32_4(SYSCALL_TRANSFER_RIGHT | (flags << 8), (uint32_t)task_group, task_group >> 32, (uint32_t)right, right >> 32);
    #else
    result = syscall2(SYSCALL_TRANSFER_RIGHT | (flags << 8), task_group, right);
    #endif
    return (right_request_t) {
        .result = static_cast<result_t>(result.result),
        .right = result.value,
    };
}

syscall_r get_mem_object_size(mem_object_t mem_object_id, unsigned flags)
{
    #ifdef __32BITSYSCALL
    return syscall32_2(SYSCALL_GET_MEM_OBJECT_SIZE | (flags << 8), (uint32_t)mem_object_id, mem_object_id >> 32);
    #else
    return syscall1(SYSCALL_GET_MEM_OBJECT_SIZE | (flags << 8), mem_object_id);
    #endif
}

page_table_req_ret_t assign_page_table(uint64_t pid, uint64_t page_table, unsigned flags, unsigned for_arch)
{
#ifdef __32BITSYSCALL
    syscall_r r = syscall32_4(SYSCALL_ASSIGN_PAGE_TABLE | (flags << 8) | (for_arch << 24), (uint32_t)pid, pid >> 32, (uint32_t)page_table, page_table >> 32);
#else
    syscall_r r = syscall2(SYSCALL_ASSIGN_PAGE_TABLE | (flags << 8) | (for_arch << 24), pid, page_table);
#endif
    return (page_table_req_ret_t) {
        .result = static_cast<result_t>(r.result),
        .page_table = r.value
    };
}

result_t release_region(uint64_t tid, void *region)
{
#ifdef __32BITSYSCALL
    return syscall32_3(SYSCALL_DELETE_REGION, (uint32_t)tid, tid >> 32, reinterpret_cast<uintptr_t>(region)).result;
#else
    return syscall2(SYSCALL_DELETE_REGION, tid, reinterpret_cast<uintptr_t>(region)).result;
#endif
}

mem_request_ret_t transfer_region(uint64_t to_page_table, void *region, uint64_t dest, uint32_t flags)
{
#ifdef __32BITSYSCALL
    syscall_r r = syscall32_5(SYSCALL_TRANSFER_REGION | (flags << 8), (uint32_t)to_page_table, to_page_table >> 32, (uint32_t)dest, dest >> 32, reinterpret_cast<uintptr_t>(region));
#else
    syscall_r r = syscall3(SYSCALL_TRANSFER_REGION | (flags << 8), to_page_table, dest, reinterpret_cast<uintptr_t>(region));
#endif
    mem_request_ret_t t = {
        .result = static_cast<result_t>(r.result),
        .virt_addr_intptr = r.value
    };
    return t;
}

syscall_r init_stack(uint64_t tid, uint64_t stack_top)
{
#ifdef __32BITSYSCALL
    return syscall32_4(SYSCALL_INIT_STACK, (uint32_t)tid, tid >> 32, (uint32_t)stack_top, stack_top >> 32);
#else
    return syscall2(SYSCALL_INIT_STACK, tid, stack_top);
#endif
}

result_t syscall_start_process(uint64_t pid, unsigned long entry, unsigned long arg1,
                               unsigned long arg2, unsigned long arg3)
{
#ifdef __32BITSYSCALL
    return syscall32_6(SYSCALL_START_PROCESS, (uint32_t)pid, pid >> 32, entry, arg1, arg2, arg3).result;
#else
    return syscall5(SYSCALL_START_PROCESS, pid, entry, arg1, arg2, arg3).result;
#endif
}

syscall_r syscall_new_process()
{
#ifdef __32BITSYSCALL
    return syscall32_0(SYSCALL_CREATE_PROCESS);
#else
    return syscall0(SYSCALL_CREATE_PROCESS);
#endif
}

result_t syscall_set_task_name(uint64_t tid, const char *name, size_t name_length)
{
#ifdef __32BITSYSCALL
    return syscall32_4(SYSCALL_SET_TASK_NAME, (uint32_t)tid, tid >> 32, (uint32_t)name, name_length).result;
#else
    return syscall3(SYSCALL_SET_TASK_NAME, tid, reinterpret_cast<uintptr_t>(name), name_length).result;
#endif
}

syscall_r create_task_group()
{
#ifdef __32BITSYSCALL
    return syscall32_0(SYSCALL_CREATE_TASK_GROUP);
#else
    return syscall0(SYSCALL_CREATE_TASK_GROUP);
#endif
}

result_t add_task_to_group(uint64_t group, uint64_t task)
{
#ifdef __32BITSYSCALL
    return syscall32_4(SYSCALL_ADD_TASK_TO_GROUP, (uint32_t)group, group >> 32, (uint32_t)task, task >> 32).result;
#else
    return syscall2(SYSCALL_ADD_TASK_TO_GROUP, group, task).result;
#endif
}

result_t remove_task_from_group(uint64_t group, uint64_t task)
{
#ifdef __32BITSYSCALL
    return syscall32_4(SYSCALL_REMOVE_TASK_FROM_GROUP, (uint32_t)group, group >> 32, (uint32_t)task, task >> 32).result;
#else
    return syscall2(SYSCALL_REMOVE_TASK_FROM_GROUP, group, task).result;
#endif
}

result_t syscall_kill_task(uint64_t tid)
{
#ifdef __32BITSYSCALL
    return syscall32_2(SYSCALL_KILL_TASK, (uint32_t)tid, tid >> 32).result;
#else
    return syscall1(SYSCALL_KILL_TASK, tid).result;
#endif
}

phys_addr_request_t get_page_phys_address(uint64_t task_id, void *region, uint64_t flags)
{
#ifdef __32BITSYSCALL
    syscall_r r =
        syscall32_4(SYSCALL_GET_PAGE_ADDRESS, (uint32_t)task_id, task_id >> 32, reinterpret_cast<uintptr_t>(region), flags);
#else
    syscall_r r = syscall3(SYSCALL_GET_PAGE_ADDRESS, task_id, reinterpret_cast<uintptr_t>(region), flags);
#endif
    phys_addr_request_t t = {static_cast<result_t>(r.result), r.value};
    return t;
}

phys_addr_request_t get_page_phys_address_from_object(mem_object_t object_id, uint64_t offset,
                                                      unsigned flags)
{
#ifdef __32BITSYSCALL
    syscall_r r = syscall32_4(SYSCALL_MEM_OBJECT_GET_PAGE_ADDRESS | (flags << 8),
                          (uint32_t)object_id, object_id >> 32, (uint32_t)offset, offset >> 32);
#else
    syscall_r r =
        syscall2(SYSCALL_MEM_OBJECT_GET_PAGE_ADDRESS | (flags << 8), object_id, offset);
#endif
    phys_addr_request_t t = {static_cast<result_t>(r.result), r.value};
    return t;
}

right_request_t create_mem_object(uint64_t size, uint32_t flags)
{
#ifdef __32BITSYSCALL
    syscall_r r = syscall32_2(SYSCALL_CREATE_MEM_OBJECT | (flags << 8), (uint32_t)size, size >> 32);
#else
    syscall_r r = syscall1(SYSCALL_CREATE_MEM_OBJECT | (flags << 8), size);
#endif
    right_request_t t = {static_cast<result_t>(r.result), r.value};
    return t;
}

right_request_t pmos_create_timer(pmos_port_t port)
{
    syscall_r result;
    #ifdef __32BITSYSCALL
    result = syscall32_2(SYSCALL_CREATE_TIMER, (uint32_t)port, port >> 32);
    #else
    result = syscall1(SYSCALL_CREATE_TIMER, port);
    #endif
    return (right_request_t) {
        .result = static_cast<result_t>(result.result),
        .right = result.value,
    };
}

result_t pmos_set_timer(pmos_port_t port, pmos_right_t timer_right, uint64_t deadline_ns, unsigned flags)
{
    #ifdef __32BITSYSCALL
    return syscall32_6(SYSCALL_SET_TIMER_DEADLINE | (flags << 8), (uint32_t)port, port >> 32, (uint32_t)timer_right, timer_right >> 32, (uint32_t)deadline_ns, deadline_ns >> 32).result;
    #else
    return syscall3(SYSCALL_SET_TIMER_DEADLINE | (flags << 8), port, timer_right, deadline_ns).result;
    #endif
}

result_t set_log_port(pmos_port_t port, uint32_t flags)
{
#ifdef __32BITSYSCALL
    return syscall32_2(SYSCALL_SET_LOG_PORT | (flags << 8), (uint32_t)port, port >> 32).result;
#else
    return syscall1(SYSCALL_SET_LOG_PORT | (flags << 8), port).result;
#endif
}

syscall_r set_task_group_notifier_mask(uint64_t task_group_id, pmos_port_t port_id,
                                       uint32_t new_mask, uint32_t flags)
{
#ifdef __32BITSYSCALL
    return syscall32_5(SYSCALL_SET_NOTIFY_MASK | (flags << 8), (uint32_t)task_group_id, task_group_id >> 32, (uint32_t)port_id, port_id >> 32, new_mask);
#else
    return syscall3(SYSCALL_SET_NOTIFY_MASK | (flags << 8), task_group_id, port_id, new_mask);
#endif
}

syscall_r get_right_type(pmos_right_t right)
{
    #ifdef __32BITSYSCALL
    return syscall32_2(SYSCALL_GET_RIGHT_TYPE, (uint32_t)right, right >> 32);
    #else
    return syscall1(SYSCALL_GET_RIGHT_TYPE, right);
    #endif
}

syscall_r set_namespace(uint64_t new_id, unsigned type)
{
#ifdef __32BITSYSCALL
    return syscall32_3(SYSCALL_SET_NAMESPACE, (uint32_t)new_id, new_id >> 32, type);
#else
    return syscall2(SYSCALL_SET_NAMESPACE, new_id, type);
#endif
}

syscall_r pmos_sleep(uint64_t nanoseconds)
{
#ifdef __32BITSYSCALL
    return syscall32_2(SYSCALL_SLEEP, (uint32_t)nanoseconds, nanoseconds >> 32);
#else
    return syscall1(SYSCALL_SLEEP, nanoseconds);
#endif
}

result_t pmos_futex_wait(int *pointer, int expected, uint64_t timeout_ns)
{
#ifdef __32BITSYSCALL
    return syscall32_4(SYSCALL_FUTEX_WAIT, (uint32_t)timeout_ns, timeout_ns >> 32, reinterpret_cast<uintptr_t>(pointer), expected).result;
#else
    return syscall3(SYSCALL_FUTEX_WAIT, timeout_ns, reinterpret_cast<uintptr_t>(pointer), expected).result;
#endif
}

result_t pmos_futex_wake(int *pointer, bool all)
{
#ifdef __32BITSYSCALL
    return syscall32_2(SYSCALL_FUTEX_WAKE, reinterpret_cast<uintptr_t>(pointer), all).result;
#else
    return syscall2(SYSCALL_FUTEX_WAKE, reinterpret_cast<uintptr_t>(pointer), all).result;
#endif
}

result_t pmos_yield()
{
#ifdef __32BITSYSCALL
    return syscall32_0(SYSCALL_YIELD).result;
#else
    return syscall0(SYSCALL_YIELD).result;
#endif
}

void pmos_syscall_exit(unsigned status, bool force)
{
#ifdef __32BITSYSCALL
    syscall32_2(SYSCALL_EXIT, status, force);
#else
    syscall2(SYSCALL_EXIT, status, force);
#endif
    __builtin_unreachable();
}

} // extern "C"