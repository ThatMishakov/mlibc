#include <assert.h>
#include <errno.h>
#include <pmos/ports.h>
#include <stdlib.h>
#include <pmos/ipc.h>
#include <alloca.h>
#include <string.h>
#include <mlibc/debug.hpp>
#include <kernel/attributes.h>
#include "common.hpp"

result_t get_message(Message_Descriptor *desc, unsigned char **message, pmos_port_t port,
                     pmos_right_t *reply_right, pmos_right_t *other_rights)
{
    result_t result = syscall_get_message_info(desc, port, 0);
    if (result != SUCCESS)
        return result;

    *message = static_cast<unsigned char *>(malloc(desc->size));
    if (*message == NULL)
        return -errno;

    if (other_rights) {
        result_t result = accept_rights(port, other_rights);
        assert(result == SUCCESS);
    }

    if (reply_right) {
        right_request_t r = get_first_message((char *)*message, 0, port);
        *reply_right = r.right;
        result = r.result;
    } else {
        result = get_first_message((char *)*message, MSG_ARG_REJECT_RIGHT, port).result;
    }
    if (result != SUCCESS) {
        free(*message);
    }

    return result;
}

right_request_t request_named_port(const char *name, size_t length, pmos_port_t reply_port, unsigned flags)
{
    size_t size = sizeof(IPC_Get_Named_Right) + length;

    IPC_Get_Named_Right *n = (IPC_Get_Named_Right *)alloca(size);
    n->type               = IPC_Get_Named_Right_NUM;
    n->flags              = flags;
    memcpy(n->name, name, length);

    return send_message_right(0, reply_port, n, size, 0, 0);
}

right_request_t get_right_by_name(const char *name, size_t length, uint32_t flags)
{
    pmos_port_t reply_port = __pmos_prepare_reply_port();
    if (reply_port == INVALID_PORT) {
        return (right_request_t) {
            .result = static_cast<result_t>(-ENOMEM),
            .right   = 0,
        };
    }

    right_request_t r_result = request_named_port(name, length, reply_port, flags);
    if (r_result.result)
        return (right_request_t) {
            .result = r_result.result,
            .right   = 0,
        };

    Message_Descriptor reply_descr;
    void *reply_msg;
    pmos_right_t rights[4] = {0};
    result_t result = get_message(&reply_descr, (unsigned char **)&reply_msg, reply_port, NULL, rights);
    if (result != SUCCESS) {
        delete_right(r_result.right);
        return (right_request_t) {
            .result = result,
            .right   = 0,
        };
    }

    if (reply_descr.size < sizeof(IPC_Named_Right_Notification)) {
        free(reply_msg);
        return (right_request_t) {
            .result = static_cast<result_t>(-EIO),
            .right   = 0,
        };
    }

    if (((IPC_Generic_Msg *)reply_msg)->type != IPC_Named_Right_Notification_NUM) {
        free(reply_msg);
        return (right_request_t) {
            .result = static_cast<result_t>(-EIO),
            .right   = 0,
        };
    }

    IPC_Named_Right_Notification *reply = (IPC_Named_Right_Notification *)reply_msg;

    right_request_t rresult = {
        .result = static_cast<result_t>(reply->result),
        .right   = rights[0],
    };

    free(reply_msg);
    return rresult;
}

result_t name_right(pmos_right_t right, const char *name, size_t length, uint32_t flags)
{
    result_t result            = 0;
    IPC_Generic_Msg *reply_msg = NULL;
    IPC_Name_Right_Reply *reply = NULL;
    result_t k_result;

    pmos_port_t reply_port = __pmos_prepare_reply_port();
    if (reply_port == INVALID_PORT) {
        return -ENOMEM;
    }

    IPC_Name_Right *n = (IPC_Name_Right *)alloca(sizeof(*n) + length);
    n->type          = IPC_Name_Right_NUM;
    n->flags         = flags;
    memcpy(n->name, name, length);

    message_extra_t extra = {
        .extra_rights = {right},
    };

    right_request_t s_result = send_message_right(0, reply_port, n, sizeof(*n) + length, &extra, 0);
    if (s_result.result != SUCCESS) {
        result = s_result.result;
        goto out;
    }

    Message_Descriptor reply_descr;
    k_result = get_message(&reply_descr, (unsigned char **)&reply_msg, reply_port, NULL, NULL);
    if (k_result != SUCCESS) {
        delete_right(s_result.right);
        result = k_result;
        goto out;
    }

    if (reply_descr.size < sizeof(IPC_Name_Right_Reply)) {
        result = -EIO;
        goto out;
    }

    if (reply_msg->type != IPC_Name_Right_Reply_NUM) {
        result = -EIO;
        goto out;
    }

    reply = (IPC_Name_Right_Reply *)reply_msg;

    result = reply->result;
out:
    free(reply_msg);
    return result;
}

extern "C" {

syscall_r __pmos_syscall_set_attr(uint64_t pid, uint32_t attr, unsigned long value);

int pmos_request_io_permission()
{
    pid_t my_pid = get_task_id();

    int64_t result = __pmos_syscall_set_attr(my_pid, ATTR_ALLOW_PORT, 1).result;
    if (result < 0) {
        errno = -result;
        return -1;
    }
    return 0;
}

}