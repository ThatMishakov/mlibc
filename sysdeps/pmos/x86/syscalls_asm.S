    .text
    .global syscall32_0
    .type syscall32_0, @function
syscall32_0:
    popl %eax
    xchgl %eax, 0(%esp)
    pushl %ebp
    movl %esp, %ebp

    pushl %ebx
    pushl %edi
    pushl %esi
    pushl %eax

    movl 8(%ebp), %eax

    int $0xf8

syscall_return:
    popl %ecx
    movl %eax, (%ecx)
    movl %ebx, 4(%ecx)
    movl %esi, 8(%ecx)
    movl %edi, 12(%ecx)
    movl %ecx, %eax

    popl %esi
    popl %edi
    popl %ebx

    popl %ebp
    ret

    .text
    .global syscall32_1
    .type syscall32_1, @function
syscall32_1:
    popl %eax
    xchgl %eax, 0(%esp)

    pushl %ebp
    movl %esp, %ebp

    pushl %ebx
    pushl %edi
    pushl %esi
    pushl %eax

    movl 8(%ebp), %eax
    movl 12(%ebp), %ebx
    
    int $0xf8

    jmp syscall_return

    .global syscall32_2
    .type syscall32_2, @function
syscall32_2:
    popl %eax
    xchgl %eax, 0(%esp)

    pushl %ebp
    movl %esp, %ebp

    pushl %ebx
    pushl %edi
    pushl %esi
    pushl %eax

    movl 8(%ebp), %eax
    movl 12(%ebp), %ebx
    movl 16(%ebp), %esi
    
    int $0xf8

    jmp syscall_return

    .global syscall32_3
    .type syscall32_3, @function
syscall32_3:
    popl %eax
    xchgl %eax, 0(%esp)

    pushl %ebp
    movl %esp, %ebp

    pushl %ebx
    pushl %edi
    pushl %esi
    pushl %eax

    movl 8(%ebp), %eax
    movl 12(%ebp), %ebx
    movl 16(%ebp), %esi
    movl 20(%ebp), %edi
    
    int $0xf8

    jmp syscall_return

    .global syscall32_4
    .type syscall32_4, @function
syscall32_4:
    popl %eax
    xchgl %eax, 0(%esp)

    pushl %ebp
    movl %esp, %ebp

    pushl %ebx
    pushl %edi
    pushl %esi
    pushl %eax

    movl 8(%ebp), %eax
    movl 12(%ebp), %ebx
    movl 16(%ebp), %esi
    movl 20(%ebp), %edi
    movl 24(%ebp), %ebp
    
    int $0xf8

    jmp syscall_return

    .global syscall32_5
    .type syscall32_5, @function
syscall32_5:
    popl %eax
    xchgl %eax, 0(%esp)

    pushl %ebp
    movl %esp, %ebp

    pushl %ebx
    pushl %edi
    pushl %esi
    pushl %eax

    pushl 28(%ebp)

    movl 8(%ebp), %eax
    movl 12(%ebp), %ebx
    movl 16(%ebp), %esi
    movl 20(%ebp), %edi
    movl 24(%ebp), %ebp
    
    int $0xf8

    addl $4, %esp

    jmp syscall_return

    .global syscall32_6
    .type syscall32_6, @function
syscall32_6:
    popl %eax
    xchgl %eax, 0(%esp)

    pushl %ebp
    movl %esp, %ebp

    pushl %ebx
    pushl %edi
    pushl %esi
    pushl %eax

    pushl 32(%ebp)
    pushl 28(%ebp)

    movl 8(%ebp), %eax
    movl 12(%ebp), %ebx
    movl 16(%ebp), %esi
    movl 20(%ebp), %edi
    movl 24(%ebp), %ebp
    
    int $0xf8

    addl $8, %esp

    jmp syscall_return

    .global syscall32_7
    .type syscall32_7, @function
syscall32_7:
    popl %eax
    xchgl %eax, 0(%esp)

    pushl %ebp
    movl %esp, %ebp

    pushl %ebx
    pushl %edi
    pushl %esi
    pushl %eax

    pushl 36(%ebp)
    pushl 32(%ebp)
    pushl 28(%ebp)

    movl 8(%ebp), %eax
    movl 12(%ebp), %ebx
    movl 16(%ebp), %esi
    movl 20(%ebp), %edi
    movl 24(%ebp), %ebp
    
    int $0xf8

    addl $12, %esp

    jmp syscall_return

    .global syscall32_8
    .type syscall32_8, @function
syscall32_8:
    popl %eax
    xchgl %eax, 0(%esp)

    pushl %ebp
    movl %esp, %ebp

    pushl %ebx
    pushl %edi
    pushl %esi
    pushl %eax

    pushl 40(%ebp)
    pushl 36(%ebp)
    pushl 32(%ebp)
    pushl 28(%ebp)

    movl 8(%ebp), %eax
    movl 12(%ebp), %ebx
    movl 26(%ebp), %esi
    movl 20(%ebp), %edi
    movl 24(%ebp), %ebp
    
    int $0xf8

    addl $16, %esp

    jmp syscall_return