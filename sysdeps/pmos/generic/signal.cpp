#include <mlibc/all-sysdeps.hpp>

#include <string.h>

#include <pmos/ipc.h>

#include "common.hpp"

namespace mlibc {

using namespace pmos;

int Sysdeps<Sigaction>::operator()(int sigval, const struct sigaction *__restrict snew, struct sigaction *__restrict sold)
{
    if (sigval <= 0 || sigval >= _NSIG)
        return EINVAL;

    if (__posix_server_right == INVALID_RIGHT)
        return ENOSYS;

    auto port = __pmos_prepare_reply_port();
    if (port == INVALID_PORT)
        return EIO;

    IPC_Sigaction msg = {};
    msg.type = IPC_Sigaction_NUM;
    msg.sigval = sigval;

    if (snew) {
        msg.flags |= SIGACTION_FLAG_SET;
        msg.sa_handler_ = reinterpret_cast<uintptr_t>(snew->sa_handler);
        msg.sa_restorer = reinterpret_cast<uintptr_t>(snew->sa_restorer);
        msg.sa_flags = snew->sa_flags;
        msg.sa_mask = snew->sa_mask;
    }

    auto send_result = send_message_right(__posix_server_right, port, &msg, sizeof(msg), nullptr, 0);
    if (send_result.result != SUCCESS)
        return kernel_to_errno(send_result.result);

    Message_Descriptor reply_descr;
    auto result = syscall_get_message_info(&reply_descr, port, 0);
    __ensure(result == SUCCESS);

    IPC_Sigaction_Reply reply;
    result = get_first_message(reinterpret_cast<char *>(&reply), MSG_ARG_REJECT_RIGHT, port).result;
    __ensure(result == SUCCESS);

    if (sold) {
        sold->sa_handler = reinterpret_cast<void (*)(int)>(static_cast<uintptr_t>(reply.old_sa_handler));
        sold->sa_restorer = reinterpret_cast<void (*)()>(static_cast<uintptr_t>(reply.old_sa_restorer));
        sold->sa_flags = reply.old_sa_flags;
        sold->sa_mask = reply.old_sa_mask;
    }

    return -reply.result;
}

} // namespace mlibc