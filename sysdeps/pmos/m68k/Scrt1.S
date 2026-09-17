.section .text
	.global _start

	.type _start, %function
	.type main, %function
	.type __mlibc_entry, %function

_start:
	move.l #0, %a6

	lea _GLOBAL_OFFSET_TABLE_@GOTPC (%pc), %a5
	move.l %sp, -(%sp)
	move.l main@GOT(%a5), -(%sp)
	jbsr __mlibc_entry@PLTPC

	.section .note.GNU-stack,"",%progbits