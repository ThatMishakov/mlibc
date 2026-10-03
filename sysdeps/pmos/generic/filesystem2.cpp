#include "common.hpp"
#include <frg/scope_exit.hpp>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <alloca.h>

namespace mlibc {

int Sysdeps<Openpty>::operator()(int *mfd, int *sfd, char *name, const struct termios *ios, const struct winsize *win) {
    int m = posix_openpt(O_RDWR | O_NOCTTY);
    if (m < 0)
        return errno;

    frg::scope_exit close_master([&] {
        close(m);
    });

    if (grantpt(m) < 0)
        return errno;
    if (unlockpt(m) < 0)
        return errno;

    auto length = sysconf(_SC_TTY_NAME_MAX);
    if (length < 0)
        return errno;

    char *slave_name = (char *)alloca(length);
    if (ptsname_r(m, slave_name, length) < 0)
        return errno;

    int s = open(slave_name, O_RDWR | O_NOCTTY);
    if (s < 0)
        return errno;

    frg::scope_exit close_slave([&] {
        close(s);
    });

    if (name)
        strcpy(name, slave_name);

    if (ios && tcsetattr(m, TCSAFLUSH, ios) < 0)
        return errno;
    if (win && ioctl(m, TIOCSWINSZ, (void*)win) < 0)
        return errno;

    *mfd = m;
    *sfd = s;
    close_master.release();
    close_slave.release();
    return 0;
}

}