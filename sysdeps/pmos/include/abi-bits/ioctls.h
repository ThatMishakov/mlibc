#ifndef _ABIBITS_IOCTLS_H
#define _ABIBITS_IOCTLS_H

#include <mlibc-config.h>

#define IOCPARM_SHIFT 14
#define IOCPARM_MASK ((1 << IOCPARM_SHIFT) - 1)
#define IOC_IN 0x80000000UL

#define _IOC(inout, group, num, len)                                                               \
	((unsigned long)((inout) | (((len) & IOCPARM_MASK) << 16) | ((group) << 8) | (num)))
#define _IOW(g, n, t) _IOC(IOC_IN, (g), (n), sizeof(t))


#define TCSETS 0x5402
#define TCSETSW 0x5403
#define TCSETSF 0x5404
#define TCGETS 0x5401

#define TIOCGPTN _IOR('T', 0x30, int)
#define TIOCSPTLCK _IOW('T', 0x31, int)

#if __MLIBC_LINUX_OPTION
#include <asm/ioctls.h>
#include <linux/sockios.h>
#endif /* __MLIBC_LINUX_OPTION */

#endif /* _ABIBITS_IOCTLS_H */