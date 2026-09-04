.section .init
	pop %eax
	ret

.section .fini
	pop %eax
	ret

.section .note.GNU-stack,"",%progbits
