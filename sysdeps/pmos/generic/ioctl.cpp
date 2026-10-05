#include <mlibc/all-sysdeps.hpp>
#include "common.hpp"
#include <frg/mutex.hpp>
#include <pmos/ipc.h>

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

    switch (request) {
    case TIOCSCTTY: {
        IPC_Ioctl message = {
            .type = IPC_Ioctl_NUM,
            .flags = flags,
            .request = request,
        };

        auto send_result = send_message_right(io_right, port, &message, sizeof(message), nullptr, 0);
        if (send_result.result != SUCCESS)
            return -send_result.result;

        Message_Descriptor reply_descr;
        auto result = syscall_get_message_info(&reply_descr, port, 0);
        __ensure(result == SUCCESS);

        IPC_Ioctl_Reply reply;
        result = get_first_message(reinterpret_cast<char *>(&reply), MSG_ARG_REJECT_RIGHT, port).result;
        __ensure(result == SUCCESS);

        if (reply_descr.size < sizeof(IPC_Generic_Msg))
            return EIO;

        if (reply.type != IPC_Ioctl_Reply_NUM)
            return EIO;

        if (reply.result_code < 0)
            return -reply.result_code;

        *result_out = reply.ioctl_result;
        return 0;
    }
        break;
    default:
        mlibc::infoLogger() << "\e[31mmlibc: unknown ioctl() request: " << request << "\e[0m" << frg::endlog;
        return ENOSYS;
    }
}

} // namespace mlibc