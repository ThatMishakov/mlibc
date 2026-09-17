.section .text
	.global _start

	.type _start, %function
	.type main, %function
	.type __mlibc_entry, %function

_start:
	move.l #0, %a6
	move.l %sp, -(%sp)
	move.l #main, -(%sp)
	jsr __mlibc_entry

	.section .note.GNU-stack,"",%progbits