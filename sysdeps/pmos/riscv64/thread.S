    .extern __mlibc_thread_trampoline
    
    .text
    .global __mlibc_thread_entry
    .type __mlibc_thread_entry, @function
__mlibc_thread_entry:
    ld a0, 0(sp)
    ld a1, 8(sp)
    ld a2, 16(sp)
    addi sp, sp, 24
    call __mlibc_thread_trampoline
    unimp

.section .note.GNU-stack,"",%progbits