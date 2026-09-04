#include <mlibc/all-sysdeps.hpp>
#include <mlibc/arch-defs.hpp>
#include <mlibc/tcb.hpp>
#include <sys/mman.h>

#include <pmos/memory.h>
#include <pmos/syscall.h>

#include "common.hpp"
#include <frg/scope_exit.hpp>


constexpr unsigned long DEFAULT_STACK = 0x200000;

constexpr uintptr_t guard_page_size = mlibc::page_size;

extern "C" void __mlibc_thread_entry();

namespace mlibc {

int Sysdeps<PrepareStack>::operator()(
    void **stack,
    void *entry,
    void *arg,
    void *tcb,
    size_t *stack_size,
    size_t *guard_size,
    void **stack_base
) {
	*guard_size = guard_page_size;

	*stack_size = *stack_size ? *stack_size : DEFAULT_STACK;

	if (!*stack) {
        auto result = Sysdeps<AnonAllocate>()(*stack_size + *guard_size, stack_base);
        if (result)
            return result;
        
        // TODO: This is not a good guard page implementation, since the kernel might allocated memory in this region
		// munmap((char *)*stack_base + *stack_size, mlibc::page_size);
	} else {
		*stack_base = *stack;
	}

	*stack = (void *)((char *)*stack_base + *stack_size);

	void **stack_it = (void **)*stack;

	*--stack_it = tcb;
	*--stack_it = arg;
	*--stack_it = entry;

	*stack = (void *)stack_it;

	return 0;
}

int Sysdeps<Clone>::operator()(void *tcb, pid_t *pid_out, void *stack) {
    (void)tcb;
    auto r = syscall_new_process();
    if (r.result != SUCCESS)
        return kernel_to_errno(r.result);

    *pid_out = (pid_t)r.value;
    frg::scope_exit error_cleanup{[=] {
        syscall_kill_task(r.value);
    }};

    auto add_result = add_task_to_group(r.value, __process_task_group);
    if (add_result != SUCCESS)
        return kernel_to_errno(add_result);

    auto table_result = assign_page_table(r.value, PAGE_TABLE_SELF, PAGE_TABLE_ASSIGN, 0);
    if (table_result.result != SUCCESS)
        return kernel_to_errno(table_result.result);

    auto stack_result = init_stack(r.value, (uint64_t)stack);
    if (stack_result.result != SUCCESS)
        return kernel_to_errno(stack_result.result);

    auto start_result = syscall_start_process(r.value, (unsigned long)__mlibc_thread_entry, 0, 0, 0);
    if (start_result != SUCCESS)
        return kernel_to_errno(start_result);

    error_cleanup.release();
    return 0;
}

[[noreturn]] void Sysdeps<ThreadExit>::operator()() {
    pmos_syscall_exit(0, false);
    __builtin_unreachable();
}

} // namespace mlibc

extern "C" void __mlibc_thread_trampoline(void *(*fn)(void *), void *user_arg, Tcb *tcb) {
    auto set_result = set_namespace(__process_task_group, NAMESPACE_RIGHTS);
    if (set_result.result != SUCCESS)
        __ensure(!"Failed to set task group namespace for new thread");

    if (mlibc::sysdep<TcbSet>(tcb))
		__ensure(!"failed to set tcb for new thread");

    while (__atomic_load_n(&tcb->tid, __ATOMIC_RELAXED) == 0)
		mlibc::sysdep<FutexWait>(&tcb->tid, 0, nullptr);

    // Enable cancellation once the TCB is up
	__atomic_fetch_or(&tcb->cancelBits, tcbCancelEnableBit, __ATOMIC_RELAXED);

	tcb->invokeThreadFunc(reinterpret_cast<void *>(fn), user_arg);
	mlibc::thread_exit(tcb->returnValue);
}