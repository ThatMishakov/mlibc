#include "mlibc-asm/helpers.h"

PROC_START(_start)
	mv s1, sp
	and sp, sp, -16

	mv a0, s1
	call relocateSelf

	mv a0, s1
	call interpreterMain

	mv s1, sp
	jr a0
PROC_END(_start)

GNU_STACK_NOTE()

