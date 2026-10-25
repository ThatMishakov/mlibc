#include <mlibc/all-sysdeps.hpp>
#include "common.hpp"
#include <frg/mutex.hpp>
#include <pmos/ipc.h>
#include <frg/vector.hpp>
#include <sys/ioctl.h>
#include <pmos/fs-data.h>
#include <alloca.h>

using namespace mlibc::pmos;

namespace mlibc {

int Sysdeps<Ioctl>::operator()(int fd, unsigned long request, void *arg, int *result_out) {
    auto res = mlibc::pmos::get_open_file(fd);
    if (!res)
        return res.error();

    pmos_right_t io_right = res.value().io_right;
    auto flags = res.value().flags;
    if (io_right == INVALID_RIGHT)
        return EBADF;

    auto port = mlibc::pmos::prepare_reply_port();
    if (port == INVALID_PORT)
        return EIO;

    frg::vector<uint8_t, SysdepsAllocator> ioctl_data(getSysdepsAllocator());

    switch (request) {
    case TIOCSCTTY: {
    }
        break;
    case TCGETS:
        break;
    case TCSETS:
    case TCSETSW:
    case TCSETSF: {
        if (!arg)
            return EINVAL;

        if (!(flags & FLAG_ISATTY))
            return ENOTTY;

        ioctl_data.resize(sizeof(struct termios));
        memcpy(ioctl_data.data(), arg, sizeof(struct termios));
    }
    case TIOCGWINSZ:
        if (!arg)
            return EINVAL;
        if (!(flags & FLAG_ISATTY))
            return ENOTTY;
        break;
    case TIOCGPGRP: {
        // Nothing
    }   break;
    case TIOCSPTLCK: {
        if (!arg)
            return EINVAL;
        ioctl_data.resize(sizeof(int));
        memcpy(ioctl_data.data(), arg, sizeof(int));
    }   break;
    default:
        mlibc::infoLogger() << "\e[31mmlibc: unknown ioctl() request: " << request << "\e[0m" << frg::endlog;
        return ENOSYS;
    }

    IPC_Ioctl message = {
        .type = IPC_Ioctl_NUM,
        .flags = flags,
        .request = request,
        .data = {},
    };
    frg::vector<uint8_t, SysdepsAllocator> message_data(getSysdepsAllocator());
    message_data.resize(sizeof(message) + ioctl_data.size());
    memcpy(message_data.data(), &message, sizeof(message));
    memcpy(message_data.data() + sizeof(message), ioctl_data.data(), ioctl_data.size());
    pmos_right_t reply_right;
    do {
        auto send_result = send_message_right(io_right, port, message_data.data(), message_data.size(), nullptr, 0);
        auto result = kernel_to_errno(send_result.result);
        reply_right = send_result.right;
        if (!result)
            break;

        if (result != ENOENT)
            return result;

        auto res = mlibc::pmos::get_open_file(fd);
        __ensure(res);

        if (res.value().io_right == io_right || res.value().io_right == INVALID_RIGHT)
            return EBADF;
        io_right = res.value().io_right;
        auto msg = reinterpret_cast<IPC_Ioctl *>(message_data.data());
        msg->flags = res.value().flags;
    } while (true);

    frg::vector<uint8_t, SysdepsAllocator> reply_data(getSysdepsAllocator());
    if (auto result = mlibc::pmos::handle_get_message_right(port, reply_right, reply_data, true); !result)
        return result.error();

    if (auto result = mlibc::pmos::check_generic_errors({reply_data.begin(), reply_data.size()}); !result)
        return result.error();

    if (reply_data.size() < sizeof(IPC_Ioctl_Reply))
        return EIO;

    IPC_Ioctl_Reply *reply = (IPC_Ioctl_Reply *)reply_data.data();

    if (reply->type != IPC_Ioctl_Reply_NUM)
        return EIO;

    if (reply->result_code < 0)
        return -reply->result_code;

    size_t data_size = reply_data.size() - sizeof(IPC_Ioctl_Reply);
    switch (request) {
    case TIOCGWINSZ: {
        struct winsize *ws = reinterpret_cast<struct winsize *>(arg);
        if (data_size < sizeof(struct winsize))
            return EIO;

        memcpy(ws, reply->data, sizeof(struct winsize));
    }
        break;
    case TCGETS: {
        struct termios *term = reinterpret_cast<struct termios *>(arg);
        if (data_size < sizeof(struct termios))
            return EIO;

        memcpy(term, reply->data, sizeof(struct termios));
    }
        break;
    default:
        break;
    }

    if (result_out)
        *result_out = reply->result_code;

    return 0;
}

} // namespace mlibc