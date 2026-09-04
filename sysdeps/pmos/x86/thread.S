    .extern __mlibc_thread_trampoline
    
    .section .text
    .global __mlibc_thread_entry
    .type __mlibc_thread_entry, @function

.cfi_startproc
 __mlibc_thread_entry:
    .cfi_undefined eip
    xorl %ebp, %ebp

    // Align the stack to 16 per the ABI...

    popl %eax // entry
    popl %ecx // user_arg
    popl %edx // tcb

    pushl $0
    pushl %edx
    pushl %ecx
    pushl %eax

    // Do a call so that the stack is aligned per the ABI
    call __mlibc_thread_trampoline@plt

    ud2
.cfi_endproc

.section .note.GNU-stack,"",%progbits