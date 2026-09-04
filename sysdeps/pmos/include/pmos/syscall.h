#ifndef _PMOS__SYSCALL_H
#define _PMOS__SYSCALL_H

#include <stdint.h>

#ifndef _DEFINED_SYSCALL_R
#define _DEFINED_SYSCALL_R
typedef struct {
    int64_t result;
    uint64_t value;
} syscall_r;
#endif

#if defined(__x86_64__)

static inline syscall_r syscall0(uint64_t call_n_flags)
{
    syscall_r ret;
    asm volatile("syscall"
                 : "=a"(ret.result), "=d"(ret.value)
                 : "a"(call_n_flags)
                 : "memory", "rcx", "r11");
    return ret;
}

static inline syscall_r syscall1(uint64_t call_n_flags, uint64_t arg1)
{
    syscall_r ret;
    asm volatile("syscall"
                 : "=a"(ret.result), "=d"(ret.value)
                 : "a"(call_n_flags), "D"(arg1)
                 : "memory", "rcx", "r11");
    return ret;
}

static inline syscall_r syscall2(uint64_t call_n_flags, uint64_t arg1, uint64_t arg2)
{
    syscall_r ret;
    asm volatile("syscall"
                 : "=a"(ret.result), "=d"(ret.value)
                 : "a"(call_n_flags), "D"(arg1), "S"(arg2)
                 : "memory", "rcx", "r11");
    return ret;
}

static inline syscall_r syscall3(uint64_t call_n_flags, uint64_t arg1, uint64_t arg2, uint64_t arg3)
{
    register uintptr_t r10 asm("r10") = arg3;

    syscall_r ret;
    asm volatile("syscall"
                 : "=a"(ret.result), "=d"(ret.value)
                 : "a"(call_n_flags), "D"(arg1), "S"(arg2), "r"(r10)
                 : "memory", "rcx", "r11");
    return ret;
}

static inline syscall_r syscall4(uint64_t call_n_flags, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4)
{
    register uintptr_t r10 asm("r10") = arg3, r8 asm("r8") = arg4;

    syscall_r ret;
    asm volatile("syscall"
                 : "=a"(ret.result), "=d"(ret.value)
                 : "a"(call_n_flags), "D"(arg1), "S"(arg2), "r"(r10), "r"(r8)
                 : "memory", "rcx", "r11");
    return ret;
}

static inline syscall_r syscall5(uint64_t call_n_flags, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4, uint64_t arg5)
{
    register uintptr_t r10 asm("r10") = arg3, r8 asm("r8") = arg4, r9 asm("r9") = arg5;

    syscall_r ret;
    asm volatile("syscall"
                 : "=a"(ret.result), "=d"(ret.value)
                 : "a"(call_n_flags), "D"(arg1), "S"(arg2), "r"(r10), "r"(r8), "r"(r9)
                 : "memory", "rcx", "r11");
    return ret;
}

#elif defined(__riscv)

static inline syscall_r syscall0(uint64_t call_n_flags)
{
    register uintptr_t a0 asm("a0") = call_n_flags, a1 asm("a1");
    asm volatile("ecall\n\t"
                 : "+r"(a0), "=r"(a1)
                 : "r"(a0)
                 : "memory");
    syscall_r ret;
    ret.result = (int64_t)a0;
    ret.value = a1;
    return ret;
}

static inline syscall_r syscall1(uint64_t call_n_flags, uint64_t arg1)
{
    register uintptr_t a0 asm("a0") = call_n_flags, a1 asm("a1") = arg1;
    asm volatile("ecall\n\t"
                 : "+r"(a0), "+r"(a1)
                 : "r"(a0), "r"(a1)
                 : "memory");
    syscall_r ret;
    ret.result = (int64_t)a0;
    ret.value = a1;
    return ret;
}

static inline syscall_r syscall2(uint64_t call_n_flags, uint64_t arg1, uint64_t arg2)
{
    register uintptr_t a0 asm("a0") = call_n_flags, a1 asm("a1") = arg1, a2 asm("a2") = arg2;
    asm volatile("ecall\n\t"
                 : "+r"(a0), "+r"(a1)
                 : "r"(a0), "r"(a1), "r"(a2)
                 : "memory");
    syscall_r ret;
    ret.result = (int64_t)a0;
    ret.value = a1;
    return ret;
}

static inline syscall_r syscall3(uint64_t call_n_flags, uint64_t arg1, uint64_t arg2, uint64_t arg3)
{
    register uintptr_t a0 asm("a0") = call_n_flags, a1 asm("a1") = arg1, a2 asm("a2") = arg2,
                               a3 asm("a3") = arg3;
    asm volatile("ecall\n\t"
                 : "+r"(a0), "+r"(a1)
                 : "r"(a0), "r"(a1), "r"(a2), "r"(a3)
                 : "memory");
    syscall_r ret;
    ret.result = (int64_t)a0;
    ret.value = a1;
    return ret;
}

static inline syscall_r syscall4(uint64_t call_n_flags, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4)
{
    register uintptr_t a0 asm("a0") = call_n_flags, a1 asm("a1") = arg1, a2 asm("a2") = arg2,
                               a3 asm("a3") = arg3, a4 asm("a4") = arg4;
    asm volatile("ecall\n\t"
                 : "+r"(a0), "+r"(a1)
                 : "r"(a0), "r"(a1), "r"(a2), "r"(a3), "r"(a4)
                 : "memory");
    syscall_r ret;
    ret.result = (int64_t)a0;
    ret.value = a1;
    return ret;
}

static inline syscall_r syscall5(uint64_t call_n_flags, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4, uint64_t arg5)
{
    register uintptr_t a0 asm("a0") = call_n_flags, a1 asm("a1") = arg1, a2 asm("a2") = arg2,
                               a3 asm("a3") = arg3, a4 asm("a4") = arg4, a5 asm("a5") = arg5;
    asm volatile("ecall\n\t"
                 : "+r"(a0), "+r"(a1)
                 : "r"(a0), "r"(a1), "r"(a2), "r"(a3), "r"(a4), "r"(a5)
                 : "memory");
    syscall_r ret;
    ret.result = (int64_t)a0;
    ret.value = a1;
    return ret;
}

#elif defined(__loongarch64)
static inline syscall_r syscall0(uint64_t call_n_flags)
{
    register uintptr_t a0 asm("a0") = call_n_flags, a1 asm("a1");
    asm volatile("syscall 0\n\t"
                 : "+r"(a0), "=r"(a1)
                 : "r"(a0)
                 : "memory");
    syscall_r ret;
    ret.result = (int64_t)a0;
    ret.value = a1;
    return ret;
}

static inline syscall_r syscall1(uint64_t call_n_flags, uint64_t arg1)
{
    register uintptr_t a0 asm("a0") = call_n_flags, a1 asm("a1") = arg1;
    asm volatile("syscall 0\n\t"
                 : "+r"(a0), "+r"(a1)
                 : "r"(a0), "r"(a1)
                 : "memory");
    syscall_r ret;
    ret.result = (int64_t)a0;
    ret.value = a1;
    return ret;
}

static inline syscall_r syscall2(uint64_t call_n_flags, uint64_t arg1, uint64_t arg2)
{
    register uintptr_t a0 asm("a0") = call_n_flags, a1 asm("a1") = arg1, a2 asm("a2") = arg2;
    asm volatile("syscall 0\n\t"
                 : "+r"(a0), "+r"(a1)
                 : "r"(a0), "r"(a1), "r"(a2)
                 : "memory");
    syscall_r ret;
    ret.result = (int64_t)a0;
    ret.value = a1;
    return ret;
}

static inline syscall_r syscall3(uint64_t call_n_flags, uint64_t arg1, uint64_t arg2, uint64_t arg3)
{
    register uintptr_t a0 asm("a0") = call_n_flags, a1 asm("a1") = arg1, a2 asm("a2") = arg2,
                               a3 asm("a3") = arg3;
    asm volatile("syscall 0\n\t"
                 : "+r"(a0), "+r"(a1)
                 : "r"(a0), "r"(a1), "r"(a2), "r"(a3)
                 : "memory");
    syscall_r ret;
    ret.result = (int64_t)a0;
    ret.value = a1;
    return ret;
}

static inline syscall_r syscall4(uint64_t call_n_flags, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4)
{
    register uintptr_t a0 asm("a0") = call_n_flags, a1 asm("a1") = arg1, a2 asm("a2") = arg2,
                               a3 asm("a3") = arg3, a4 asm("a4") = arg4;
    asm volatile("syscall 0\n\t"
                 : "+r"(a0), "+r"(a1)
                 : "r"(a0), "r"(a1), "r"(a2), "r"(a3), "r"(a4)
                 : "memory");
    syscall_r ret;
    ret.result = (int64_t)a0;
    ret.value = a1;
    return ret;
}

static inline syscall_r syscall5(uint64_t call_n_flags, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4, uint64_t arg5)
{
    register uintptr_t a0 asm("a0") = call_n_flags, a1 asm("a1") = arg1, a2 asm("a2") = arg2,
                               a3 asm("a3") = arg3, a4 asm("a4") = arg4, a5 asm("a5") = arg5;
    asm volatile("syscall 0\n\t"
                 : "+r"(a0), "+r"(a1)
                 : "r"(a0), "r"(a1), "r"(a2), "r"(a3), "r"(a4), "r"(a5)
                 : "memory");
    syscall_r ret;
    ret.result = (int64_t)a0;
    ret.value = a1;
    return ret;
}

#elif defined(__i386__)

extern "C" {

syscall_r syscall32_0(uint32_t call_n_flags);
syscall_r syscall32_1(uint32_t call_n_flags, uint32_t arg1);
syscall_r syscall32_2(uint32_t call_n_flags, uint32_t arg1, uint32_t arg2);
syscall_r syscall32_3(uint32_t call_n_flags, uint32_t arg1, uint32_t arg2, uint32_t arg3);
syscall_r syscall32_4(uint32_t call_n_flags, uint32_t arg1, uint32_t arg2, uint32_t arg3, uint32_t arg4);
syscall_r syscall32_5(uint32_t call_n_flags, uint32_t arg1, uint32_t arg2, uint32_t arg3, uint32_t arg4, uint32_t arg5);
syscall_r syscall32_6(uint32_t call_n_flags, uint32_t arg1, uint32_t arg2, uint32_t arg3, uint32_t arg4, uint32_t arg5, uint32_t arg6);
syscall_r syscall32_7(uint32_t call_n_flags, uint32_t arg1, uint32_t arg2, uint32_t arg3, uint32_t arg4, uint32_t arg5, uint32_t arg6, uint32_t arg7);
syscall_r syscall32_8(uint32_t call_n_flags, uint32_t arg1, uint32_t arg2, uint32_t arg3, uint32_t arg4, uint32_t arg5, uint32_t arg6, uint32_t arg7, uint32_t arg8);

}
#else
#error "Unsupported architecture"
#endif

#endif /* _PMOS__SYSCALL_H */