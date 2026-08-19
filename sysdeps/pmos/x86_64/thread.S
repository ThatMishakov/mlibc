    .extern __mlibc_thread_trampoline
    
    .section .text
    .global __mlibc_thread_entry
    .type __mlibc_thread_entry, @function
 __mlibc_thread_entry:
    popq %rdi // entry
    popq %rsi // user_arg
    popq %rdx // tcb

    // Do a call so that the stack is aligned per the ABI
    call __mlibc_thread_trampoline

    ud2

.section .note.GNU-stack,"",%progbits

