#include <mlibc/debug.hpp>
#include <mlibc/all-sysdeps.hpp>
#include <mlibc/allocator.hpp>

#include <frg/array.hpp>
#include <frg/scope_exit.hpp>
#include <frg/eternal.hpp>

#include <pmos/system.h>
#include <mlibc/threads.hpp>
#include <pmos/fs-data.h>
#include <pmos/ipc.h>
#include <pmos/ports.h>

#include <sys/auxv.h>
#include <sys/mman.h>
#include <alloca.h>

#include "common.hpp"

#define STUB()                                                                                     \
    ({                                                                                             \
        __ensure(!"STUB function was called");                                                     \
        __builtin_unreachable();                                                                   \
})

uint64_t __process_task_group;
pmos_right_t __posix_server_right = INVALID_RIGHT;
extern uintptr_t *entryStack;

namespace {

struct RightWrapper {
    pmos_right_t right = INVALID_RIGHT;
    ~RightWrapper() {
        if (right != INVALID_RIGHT)
            delete_right(right);
    }

    RightWrapper() = default;
    RightWrapper(pmos_right_t r) : right(r) {}
    RightWrapper(RightWrapper &&other) : right(other.right) {
        other.right = INVALID_RIGHT;
    }
    RightWrapper &operator=(RightWrapper &&other) {
        if (this != &other) {
            if (right != INVALID_RIGHT)
                delete_right(right);
            right = other.right;
            other.right = INVALID_RIGHT;
        }
        return *this;
    }
    RightWrapper(const RightWrapper &) = delete;
    RightWrapper &operator=(const RightWrapper &) = delete;
};

struct OpenFile {
    pmos_right_t io_right;
    pmos_right_t op_right;
    unsigned flags;
};

// constexpr unsigned FLAG_ISATTY = 0x01;

constinit FutexLock filesystem_mutex;
// Don't bother freeing this, notably this is needed for ld.so
constinit frg::array<OpenFile, __MLIBC_OPEN_MAX> open_files{};


unsigned flags_to_io(unsigned fd_flags) {
    unsigned io_flags = 0;
    if (fd_flags & O_APPEND)
        io_flags |= IPC_FLAG_IO_OP_APPEND;
    if (fd_flags & O_NONBLOCK)
        io_flags |= IPC_FLAG_IO_OP_NONBLOCK;
    return io_flags;
}

void init_namespace() {
    unsigned long value;
    int result = peekauxval(AT_TASK_GROUP_ID, &value);
    if (result < 0) {
        auto result = create_task_group();
        if (result.result != SUCCESS)
            __ensure(!"Failed to create task group during mlibc init");

        __process_task_group = result.value;
    } else {
        __process_task_group = *(uint64_t *)value;
    }

    auto set_result = set_namespace(__process_task_group, NAMESPACE_RIGHTS);
    if (set_result.result != SUCCESS)
        __ensure(!"Failed to set task group namespace during mlibc init");
}

void init_posix_right() {
    unsigned long value;
    int result = peekauxval(AT_POSIX_RIGHT, &value);
    if (result < 0) {
        __posix_server_right = INVALID_RIGHT;
    } else {
        __posix_server_right = *(uint64_t *)value;
    }
}

void fill_fd_table(void *fs_data_ptr) {
    auto fs_data = (struct PmosFsData *)fs_data_ptr;
    auto count = fs_data->array_size;
    __ensure(count <= __MLIBC_OPEN_MAX);
    for (unsigned i = 0; i < count; ++i) {
        auto &file = fs_data->open_files[i];

        open_files[i].io_right = file.io_right;
        open_files[i].op_right = file.op_right;
        open_files[i].flags    = file.flags;
    }

    mlibc::Sysdeps<AnonFree>()(reinterpret_cast<void *>(fs_data), fs_data->total_size);
}

void init_fs() {
    unsigned long value;
    int auxv_result = peekauxval(AT_FD_TABLE, &value);
    if (!auxv_result) {
        fill_fd_table((void *)value);
    }
}

__attribute__((constructor(49))) void init_sysdeps() {    
    init_namespace();
    init_posix_right();
    init_fs();
}

} // namespace


namespace mlibc {

int Sysdeps<Close>::operator()(int) {
    STUB();
}

int Sysdeps<Write>::operator()(int fd, const void *buff, size_t count, ssize_t *bytes_written) {
    if (fd >= __MLIBC_OPEN_MAX || fd < 0)
        return EBADF;

    pmos_right_t io_right;
    {
        frg::unique_lock lock(filesystem_mutex);
        io_right = open_files[fd].io_right;
    }

    if (io_right == INVALID_RIGHT)
        return EBADF;

    auto port = __pmos_prepare_reply_port();
    if (port == INVALID_PORT)
        return EIO;

    IPC_Write *write_msg = (IPC_Write *)alloca(sizeof(IPC_Write) + count);
    write_msg->type = IPC_Write_NUM;
    write_msg->flags = flags_to_io(open_files[fd].flags);
    write_msg->offset = 0;
    memcpy(write_msg->data, buff, count);

    auto send_result = send_message_right(io_right, port, write_msg, sizeof(IPC_Write) + count, nullptr, 0);
    if (send_result.result != SUCCESS)
        return -send_result.result;

    Message_Descriptor reply_descr;
    auto result = syscall_get_message_info(&reply_descr, port, 0);
    // TODO: Handle EINTR
    __ensure(result == SUCCESS);

    IPC_Write_Reply reply;
    result = get_first_message(reinterpret_cast<char *>(&reply), MSG_ARG_REJECT_RIGHT, port).result;
    __ensure(result == SUCCESS);

    if (reply_descr.size < sizeof(IPC_Generic_Msg))
        return EIO;

    if (reply.type != IPC_Write_Reply_NUM)
        return EIO;

    *bytes_written = static_cast<ssize_t>(reply.bytes_written);
    return -reply.result_code;
}

int Sysdeps<Read>::operator()(int , void *, size_t , ssize_t *) {
    STUB();
}

int Sysdeps<Seek>::operator()(int fd, off_t offset, int whence, off_t *new_offset) {
    if (fd >= __MLIBC_OPEN_MAX || fd < 0)
        return EBADF;

    pmos_right_t io_right;
    unsigned flags;
    {
        frg::unique_lock lock(filesystem_mutex);
        io_right = open_files[fd].io_right;
        flags = open_files[fd].flags;
    }

    if (io_right == INVALID_RIGHT)
        return EBADF;

    if (flags & FLAG_ISPIPE)
        return ESPIPE;

    auto port = __pmos_prepare_reply_port();
    if (port == INVALID_PORT)
        return EIO;

    IPC_Seek seek_msg = {
        .type = IPC_Seek_NUM,
        .flags = 0,
        .whence = static_cast<uint16_t>(whence),
        .offset = static_cast<uint64_t>(offset),
    };
    
    auto send_result = send_message_right(io_right, port, &seek_msg, sizeof(seek_msg), nullptr, 0);
    if (send_result.result != SUCCESS)
        return -send_result.result;

    Message_Descriptor reply_descr;
    auto result = syscall_get_message_info(&reply_descr, port, 0);
    __ensure(result == SUCCESS);

    IPC_Seek_Reply reply;
    result = get_first_message(reinterpret_cast<char *>(&reply), MSG_ARG_REJECT_RIGHT, port).result;
    __ensure(result == SUCCESS);

    if (reply_descr.size < sizeof(IPC_Generic_Msg))
        return EIO;

    if (reply.type != IPC_Seek_Reply_NUM)
        return EIO;

    *new_offset = static_cast<off_t>(reply.new_offset);
    return -reply.result_code;
}

int Sysdeps<Open>::operator()(const char *pathname, int flags, mode_t mode, int *fd) {
    size_t path_len = strlen(pathname);
    if (path_len > PATH_MAX)
        return ENAMETOOLONG;

    size_t message_size = sizeof(IPC_Open) + path_len;
    IPC_Open *message = (IPC_Open *)alloca(message_size);
    message->type = IPC_Open_NUM;
    message->flags = flags | mode;
    memcpy(message->path, pathname, path_len);

    auto port = __pmos_prepare_reply_port();
    if (port == INVALID_PORT)
        return EIO;

    auto send_result = send_message_right(__posix_server_right, port, message, message_size, nullptr, 0);
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
            open_files[i].io_right = extra_rights[0];
            open_files[i].op_right = extra_rights[1];
            open_files[i].flags    = flags;
            *fd = i;

            extra_rights[0] = INVALID_RIGHT;
            extra_rights[1] = INVALID_RIGHT;

            return 0;
        }
    }

    return EMFILE;
}

int Sysdeps<Isatty>::operator()(int fd) {

    if (fd >= __MLIBC_OPEN_MAX || fd < 0)
        return EBADF;

    const auto &file = open_files[fd];
    frg::unique_lock lock(filesystem_mutex);
    if (file.io_right == INVALID_RIGHT)
        return EBADF;

    if (!(file.flags & FLAG_ISATTY))
        return ENOTTY;

    return 0;
}

int Sysdeps<Recvfrom>::operator()(int , void *, size_t , int , struct sockaddr *, socklen_t *, ssize_t *) {
    STUB();
}

int Sysdeps<Dup2>::operator()(int , int , int) {
    STUB();
}

int Sysdeps<VmMap>::operator()(void *, size_t , int , int , int , off_t , void **) {
    STUB();
}

int Sysdeps<Sleep>::operator()(time_t *secs, long *nanos)
{
    constexpr uint32_t nanosecs_in_second = 1'000'000'000;

    uint64_t time = *nanos + static_cast<uint64_t>((*secs)*nanosecs_in_second);
    auto result = pmos_sleep(time);
    if (result.result) {
        *secs = result.value / nanosecs_in_second;
        *nanos = static_cast<long>(result.value % nanosecs_in_second);
    } else {
        *secs = 0;
        *nanos = 0;
    }
    return -result.result;
}

}

pmos_port_t __pmos_prepare_reply_port()
{
    auto tcb = mlibc::get_current_tcb();
    auto &port = tcb->sysdepData.threadPort;

    if (port != INVALID_PORT)
        return port;

    ports_request_t port_request = create_port(TASK_ID_SELF, 0);
    __ensure(port_request.result == SUCCESS);
    
    return port = port_request.port;
}
