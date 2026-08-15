#include <assert.h>
#include <errno.h>
#include <pmos/ports.h>
#include <stdlib.h>

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