#ifndef _ABIBITS_IOCTLS_H
#define _ABIBITS_IOCTLS_H

#include <mlibc-config.h>

#define TCSETS 0x5402
#define TCSETSW 0x5403
#define TCSETSF 0x5404

#if __MLIBC_LINUX_OPTION
#include <asm/ioctls.h>
#include <linux/sockios.h>
#endif /* __MLIBC_LINUX_OPTION */

#endif /* _ABIBITS_IOCTLS_H */