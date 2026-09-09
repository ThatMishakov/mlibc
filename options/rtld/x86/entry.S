#include "mlibc-asm/helpers.h"

PROC_START(_start)
.cfi_undefined eip
	mov %esp, %esi
	xor %ebp, %ebp

	and $-16, %esp
	lea -12(%esp), %esp
	push %esi

	call relocateSelf
	lea 4(%esp), %esp

	push %esi
	call interpreterMain

	mov %esi, %esp

	jmp *%eax
PROC_END(_start)

GNU_STACK_NOTE()
