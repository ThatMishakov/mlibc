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
    pushl %eax  // entry_stack
    pushl $main // main_fn
	call __mlibc_entry
    ud2
.cfi_endproc

.section .note.GNU-stack,"",%progbits