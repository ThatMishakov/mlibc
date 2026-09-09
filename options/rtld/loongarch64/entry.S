#include "mlibc-asm/helpers.h"

PROC_START(_start)
	move $fp, $zero
	move $s1, $sp

	li.d $t0, -16
	and $sp, $sp, $t0

	move $a0, $s1
	bl relocateSelf

	move $a0, $s1
	bl interpreterMain

	move $sp, $s1

	jr $a0
PROC_END(_start)

GNU_STACK_NOTE()

