    .extern __mlibc_thread_trampoline
    
    .section .text
    .global __mlibc_thread_entry
    .type __mlibc_thread_entry, @function

.cfi_startproc
 __mlibc_thread_entry:
    ldp x0, x1, [sp], #16
    ldr x2, [sp], #8
    bl __mlibc_thread_trampoline
    brk #0
.cfi_endproc

.section .note.GNU-stack,"",%progbits