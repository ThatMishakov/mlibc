    .section .text

    .extern __mlibc_entry

    .global _start
    .type _start, @function

.cfi_startproc
_start:
.cfi_undefined eip
    xorl %ebp, %ebp
    movl %esp, %eax

    call 1f
1:
    popl %ebx
    leal _GLOBAL_OFFSET_TABLE_+[.-1b](%ebx), %ebx

    subl $8, %esp
    pushl main@GOT(%ebx)
    pushl %eax  // entry_stack

	call __mlibc_entry@plt
    ud2
.cfi_endproc

.section .note.GNU-stack,"",%progbits