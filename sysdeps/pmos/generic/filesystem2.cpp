#include "common.hpp"
#include <frg/scope_exit.hpp>
#include <frg/mutex.hpp>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <alloca.h>

using namespace mlibc::pmos;

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

int Sysdeps<Openpt>::operator()(int oflags, int *fd) {
    IPC_Openpt message = {
        .type = IPC_Openpt_NUM,
        .flags = static_cast<uint32_t>(oflags),
    };

    auto port = __pmos_prepare_reply_port();
    if (port == INVALID_PORT)
        return EIO;

    auto send_result = send_message_right(__posix_server_right, port, &message, sizeof(message), nullptr, 0);
    if (send_result.result != SUCCESS)
        return -send_result.result;

    Message_Descriptor reply_descr;
    auto result = syscall_get_message_info(&reply_descr, port, 0);
    __ensure(result == SUCCESS);

    pmos_right_t extra_rights[4] = {};
    auto get_result = accept_rights(port, extra_rights);
    __ensure(get_result == SUCCESS);
    frg::scope_exit delete_rights([&] {
        for (size_t i = 0; i < 4; ++i) {
            if (extra_rights[i] != INVALID_RIGHT) {
                delete_right(extra_rights[i]);
            }
        }
    });

    IPC_Open_Reply reply;
    result = get_first_message(reinterpret_cast<char *>(&reply), MSG_ARG_REJECT_RIGHT, port).result;
    __ensure(result == SUCCESS);

    if (reply_descr.size < sizeof(IPC_Generic_Msg))
        return EIO;

    if (reply.type != IPC_Open_Reply_NUM)
        return EIO;

    if (reply.result_code < 0)
        return -reply.result_code;

    frg::unique_lock lock(filesystem_mutex);
    for (unsigned i = 0; i < __MLIBC_OPEN_MAX; ++i) {
        if (open_files[i].io_right == INVALID_RIGHT) {
            open_files[i].io_right = extra_rights[1];
            open_files[i].op_right = extra_rights[0];
            open_files[i].flags    = reply.fs_flags;
            *fd = i;

            extra_rights[0] = INVALID_RIGHT;
            extra_rights[1] = INVALID_RIGHT;

            return 0;
        }
    }

    return EMFILE;
}

int Sysdeps<Unlockpt>::operator()(int fd) {
    if (fd >= __MLIBC_OPEN_MAX || fd < 0)
        return EBADF;

    pmos_right_t op_right;
    {
        frg::unique_lock lock(filesystem_mutex);
        op_right = open_files[fd].op_right;
    }

    if (op_right == INVALID_RIGHT)
        return EBADF; 

    IPC_Unlockpt message = {
        .type = IPC_Unlockpt_NUM,
        .flags = 0,
    };

    auto port = __pmos_prepare_reply_port();
    if (port == INVALID_PORT)
        return EIO;

    auto send_result = send_message_right(op_right, port, &message, sizeof(message), nullptr, 0);
    if (send_result.result != SUCCESS)
        return -send_result.result;

    Message_Descriptor reply_descr;
    auto result = syscall_get_message_info(&reply_descr, port, 0);
    __ensure(result == SUCCESS);

    IPC_Unlockpt_Reply reply;
    result = get_first_message(reinterpret_cast<char *>(&reply), MSG_ARG_REJECT_RIGHT, port).result;
    __ensure(result == SUCCESS);

    if (reply_descr.size < sizeof(IPC_Generic_Msg))
        return EIO;

    if (reply.type != IPC_Unlockpt_Reply_NUM)
        return EIO;

    return -reply.result_code;
}

int Sysdeps<Ptsname>::operator()(int fd, char *buff, size_t size) {
	return sysdep<Ttyname>(fd, buff, size);
}

int Sysdeps<Ttyname>::operator()(int fd, char *buff, size_t size) {
    if (fd >= __MLIBC_OPEN_MAX || fd < 0)
        return EBADF;

    pmos_right_t op_right;
    {
        frg::unique_lock lock(filesystem_mutex);
        op_right = open_files[fd].op_right;
    }

    if (op_right == INVALID_RIGHT)
        return EBADF;

    IPC_Ttyname message = {
        .type = IPC_Ttyname_NUM,
        .flags = 0,
    };

    auto port = __pmos_prepare_reply_port();
    if (port == INVALID_PORT)
        return EIO;

    auto send_result = send_message_right(op_right, port, &message, sizeof(message), nullptr, 0);
    if (send_result.result != SUCCESS)
        return -send_result.result;

    Message_Descriptor reply_descr;
    auto result = syscall_get_message_info(&reply_descr, port, 0);
    __ensure(result == SUCCESS);

    auto *reply = reinterpret_cast<IPC_Ttyname_Reply *>(alloca(reply_descr.size));
    result = get_first_message(reinterpret_cast<char *>(reply), MSG_ARG_REJECT_RIGHT, port).result;
    __ensure(result == SUCCESS);

    if (reply_descr.size < sizeof(IPC_Generic_Msg))
        return EIO;

    if (reply->type != IPC_Ttyname_Reply_NUM)
        return EIO;

    if (reply->result_code < 0)
        return -reply->result_code;

    size_t name_length = reply_descr.size - sizeof(IPC_Ttyname_Reply);
    if (name_length - 1 > size)
        return ERANGE;

    memcpy(buff, reply->tty_name, name_length);
    buff[name_length - 1] = '\0';

    return 0;
}

} // namespace mlibc