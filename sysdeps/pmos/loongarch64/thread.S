    .extern __mlibc_thread_trampoline
    
    .text
    .global __mlibc_thread_entry
    .type __mlibc_thread_entry, @function
__mlibc_thread_entry:
	ld.d $a0, $sp, 0
	ld.d $a1, $sp, 8
	ld.d $a2, $sp, 16
	addi.d $sp, $sp, 24
	b __mlibc_thread_trampoline
	break 0

.section .note.GNU-stack,"",%progbits