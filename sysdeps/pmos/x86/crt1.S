    .section .text

    .extern __mlibc_entry

    .global _start
    .type _start, @function

.cfi_startproc
_start:
.cfi_undefined eip
    xorl %ebp, %ebp

    movl %esp, %eax

    subl $8, %esp
    pushl $main // main_fn
    pushl %eax  // entry_stack
	call __mlibc_entry
    ud2
.cfi_endproc

.section .note.GNU-stack,"",%progbits