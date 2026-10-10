#include <mlibc/all-sysdeps.hpp>
#include "common.hpp"
#include <frg/mutex.hpp>
#include <pmos/ipc.h>
#include <frg/vector.hpp>
#include <mlibc/allocator.hpp>
#include <sys/ioctl.h>
#include <pmos/fs-data.h>
#include <alloca.h>

using namespace mlibc::pmos;

namespace mlibc {

int Sysdeps<Ioctl>::operator()(int fd, unsigned long request, void *arg, int *result_out) {
    if (fd >= __MLIBC_OPEN_MAX || fd < 0)
        return EBADF;

    pmos_right_t io_right;
    unsigned flags;
    {
        frg::unique_lock lock(filesystem_mutex);
        io_right = open_files[fd].io_right;
        flags = open_files[fd].flags;
    }

    auto port = __pmos_prepare_reply_port();
    if (port == INVALID_PORT)
        return EIO;

    frg::vector<uint8_t, MemoryAllocator> ioctl_data(getAllocator());

    switch (request) {
    case TIOCSCTTY: {
    }
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
    frg::vector<uint8_t, MemoryAllocator> message_data(getAllocator());
    message_data.resize(sizeof(message) + ioctl_data.size());
    memcpy(message_data.data(), &message, sizeof(message));
    memcpy(message_data.data() + sizeof(message), ioctl_data.data(), ioctl_data.size());

    auto send_result = send_message_right(io_right, port, message_data.data(), message_data.size(), nullptr, 0);
    if (send_result.result != SUCCESS)
        return -send_result.result;

    Message_Descriptor reply_descr;
    auto result = syscall_get_message_info(&reply_descr, port, 0);
    __ensure(result == SUCCESS);

    IPC_Ioctl_Reply *reply = reinterpret_cast<IPC_Ioctl_Reply *>(alloca(reply_descr.size));
    result = get_first_message(reinterpret_cast<char *>(reply), MSG_ARG_REJECT_RIGHT, port).result;
    __ensure(result == SUCCESS);

    if (reply_descr.size < sizeof(IPC_Generic_Msg))
        return EIO;

    if (reply->type != IPC_Ioctl_Reply_NUM)
        return EIO;

    if (reply->result_code < 0)
        return -reply->result_code;

    size_t data_size = reply_descr.size - sizeof(IPC_Ioctl_Reply);
    switch (request) {
    case TIOCGWINSZ: {
        struct winsize *ws = reinterpret_cast<struct winsize *>(arg);
        if (data_size < sizeof(struct winsize))
            return EIO;

        memcpy(ws, reply->data, sizeof(struct winsize));
    }
        break;
    default:
        break;
    }

    *result_out = reply->result_code;

    return 0;
}

} // namespace mlibc