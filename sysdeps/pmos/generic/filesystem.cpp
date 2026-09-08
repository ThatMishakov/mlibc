#include <mlibc/debug.hpp>
#include <mlibc/all-sysdeps.hpp>
#include <mlibc/allocator.hpp>

#include <frg/array.hpp>
#include <frg/scope_exit.hpp>
#include <frg/eternal.hpp>
#include <frg/vector.hpp>
#include <frg/expected.hpp>

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

namespace {

unsigned flags_to_io(unsigned fd_flags) {
    unsigned io_flags = 0;
    if (fd_flags & O_APPEND)
        io_flags |= IPC_FLAG_IO_OP_APPEND;
    if (fd_flags & O_NONBLOCK)
        io_flags |= IPC_FLAG_IO_OP_NONBLOCK;
    return io_flags;
}

}

namespace mlibc::pmos {

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

#if defined(MLIBC_STATIC_BUILD) || defined(MLIBC_BUILDING_RTLD)

[[ gnu::visibility("protected") ]]
constinit FutexLock filesystem_mutex;
// Don't bother freeing this, notably this is needed for ld.so
[[ gnu::visibility("protected") ]]
constinit frg::array<OpenFile, __MLIBC_OPEN_MAX> open_files{};

[[ gnu::visibility("protected") ]]
uint64_t __process_task_group;
[[ gnu::visibility("protected") ]]
pmos_right_t __posix_server_right = INVALID_RIGHT;

namespace {

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
    // Constructors within ld.so run twice (which is probably a bug)

    static bool initialized = false;
    if (initialized)
        return;
    initialized = true;

    init_namespace();
    init_posix_right();
    init_fs();
}

} // namespace

#else

extern FutexLock filesystem_mutex;
// Don't bother freeing this, notably this is needed for ld.so
extern frg::array<OpenFile, __MLIBC_OPEN_MAX> open_files;

#endif

} // namespace mlibc::pmos

namespace mlibc {

using namespace pmos;

int Sysdeps<Close>::operator()(int fd) {
    if (fd >= __MLIBC_OPEN_MAX || fd < 0)
        return EBADF;

    pmos_right_t io_right, file_right;
    {
        frg::unique_lock lock(filesystem_mutex);
        io_right = open_files[fd].io_right;
        file_right = open_files[fd].op_right;
        open_files[fd].io_right = INVALID_RIGHT;
        open_files[fd].op_right = INVALID_RIGHT;
    }

    if (io_right == INVALID_RIGHT)
        return EBADF;

    delete_right(io_right);
    delete_right(file_right);
    return 0;
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
        return kernel_to_errno(send_result.result);

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

int Sysdeps<Read>::operator()(int fd, void *buff, size_t count, ssize_t *bytes_read) {
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

    IPC_Read read_msg = {
        .type = IPC_Read_NUM,
        .flags = flags_to_io(open_files[fd].flags),
        .start_offset = 0, // This is ignored since there's no fixed offset flag set
        .max_size = count,
    };

    auto send_result = send_message_right(io_right, port, &read_msg, sizeof(read_msg), nullptr, 0);
    if (send_result.result != SUCCESS)
        return kernel_to_errno(send_result.result);

    Message_Descriptor reply_descr;
    auto result = syscall_get_message_info(&reply_descr, port, 0);
    // TODO: Handle EINTR (as above)
    __ensure(result == SUCCESS);

    frg::vector<uint8_t, MemoryAllocator> reply_data(getAllocator());
    reply_data.resize(reply_descr.size);
    result = get_first_message(reinterpret_cast<char *>(reply_data.data()), MSG_ARG_REJECT_RIGHT, port).result;
    __ensure(result == SUCCESS);

    if (reply_descr.size < sizeof(IPC_Generic_Msg))
        return EIO;

    IPC_Read_Reply *reply = (IPC_Read_Reply *)reply_data.data();

    if (reply->type != IPC_Read_Reply_NUM)
        return EIO;

    if (reply->result_code)
        return -reply->result_code;

    *bytes_read = reply_descr.size - sizeof(IPC_Read_Reply);
    memcpy(buff, reply->data, *bytes_read);
    return 0;
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
        .offset = offset,
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
            open_files[i].io_right = extra_rights[1];
            open_files[i].op_right = extra_rights[0];
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

int Sysdeps<VmMap>::operator()(void *hint, size_t size, int prot, int flags, int fd, off_t offset, void **window) {
    unsigned map_flags = prot | mmap_flags_to_kernel(flags);

    if (flags & MAP_ANON) {
        auto result = create_normal_region(TASK_ID_SELF, hint, size, map_flags);
        if (result.result != SUCCESS) {
            return kernel_to_errno(result.result);
        }
        *window = result.virt_addr;
        return 0;
    }

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

    IPC_Get_Object msg = {
        .type = IPC_Get_Object_NUM,
        .flags = 0,
    };

    auto send_result = send_message_right(io_right, port, &msg, sizeof(msg), nullptr, 0);
    if (send_result.result != SUCCESS)
        return -send_result.result;

    Message_Descriptor reply_descr;
    auto result = syscall_get_message_info(&reply_descr, port, 0);
    // TODO: Handle EINTR
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

    IPC_Get_Object_Reply reply;
    result = get_first_message(reinterpret_cast<char *>(&reply), MSG_ARG_REJECT_RIGHT, port).result;
    __ensure(result == SUCCESS);

    if (reply_descr.size < sizeof(IPC_Generic_Msg))
        return EIO;

    if (reply.type != IPC_Get_Object_Reply_NUM)
        return EIO;

    if (reply.result_code < 0)
        return -reply.result_code;

    pmos_right_t mem_object = extra_rights[0];

    map_mem_object_param_t param = {
        .page_table_id = 0,
        .object_right = mem_object,
        .addr_start_uint = (uintptr_t)hint,
        .size = size,
        .offset_object = static_cast<uint64_t>(offset),
        .offset_start = 0,
        .object_size = size,
        .access_flags = map_flags,
    };

    auto map_result = map_mem_object(&param);
    if (map_result.result != SUCCESS) {
        return kernel_to_errno(map_result.result);
    }

    *window = map_result.virt_addr;
    return 0;
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

int Sysdeps<Stat>::operator()(fsfd_target fsfdt, int fd, const char *path, int flags, struct stat *statbuf) {
    if (!statbuf)
        return EINVAL;

    pmos_right_t file_right;

    switch (fsfdt) {
        case fsfd_target::path:
            file_right = __posix_server_right;
            break;
        case fsfd_target::fd:
            flags = AT_EMPTY_PATH;
            [[fallthrough]];

        case fsfd_target::fd_path:
            if (fd >= __MLIBC_OPEN_MAX || fd < 0)
                return EBADF;
            
            {
                frg::unique_lock lock(filesystem_mutex);
                file_right = open_files[fd].op_right;
            }
            if (file_right == INVALID_RIGHT)
                return EBADF;
            break;
        default:
            return ENOSYS;
    }

    auto port = __pmos_prepare_reply_port();
    if (port == INVALID_PORT)
        return EIO;

    size_t path_len = strlen(path);
    size_t message_size = sizeof(IPC_Stat) + path_len;
    IPC_Stat *message = (IPC_Stat *)alloca(message_size);
    message->type = IPC_Stat_NUM;
    message->flags = flags;
    memcpy(message->path, path, path_len);

    auto send_result = send_message_right(file_right, port, message, message_size, nullptr, 0);
    if (send_result.result != SUCCESS)
        return -send_result.result;

    Message_Descriptor reply_descr;
    auto result = syscall_get_message_info(&reply_descr, port, 0);
    __ensure(result == SUCCESS);

    IPC_Stat_Reply reply;
    result = get_first_message(reinterpret_cast<char *>(&reply), MSG_ARG_REJECT_RIGHT, port).result;
    __ensure(result == SUCCESS);

    if (reply_descr.size < sizeof(IPC_Generic_Msg))
        return EIO;

    if (reply.type != IPC_Stat_Reply_NUM)
        return EIO;

    if (reply.result < 0)
        return -reply.result;

    statbuf->st_dev = reply.st_dev;
    statbuf->st_ino = reply.st_ino;
    statbuf->st_nlink = reply.st_nlink;
    statbuf->st_mode = reply.st_mode;
    statbuf->st_uid = reply.st_uid;
    statbuf->st_gid = reply.st_gid;
    statbuf->st_rdev = reply.st_rdev;
    statbuf->st_size = reply.st_size;
    statbuf->st_blksize = reply.st_blksize;
    statbuf->st_blocks = reply.st_blocks;

    statbuf->st_atim = kernel_to_timespec(reply.st_atim_tv_nsec);
    statbuf->st_mtim = kernel_to_timespec(reply.st_mtim_tv_nsec);
    statbuf->st_ctim = kernel_to_timespec(reply.st_ctim_tv_nsec);

    return 0;
}

int Sysdeps<Poll>::operator()(struct pollfd *fds, nfds_t count, int timeout, int *num_events) {
    struct timespec ts;
    auto ms = timeout % 1000;
    ts.tv_sec = timeout / 1000;
    ts.tv_nsec = ms * 1000000;
    return Sysdeps<Ppoll>()(fds, count, timeout >= 0 ? &ts : nullptr, nullptr, num_events);
}

struct ReceiveRightHandler {
    pmos_right_t port = 0;
    pmos_right_t right = INVALID_RIGHT;

    ReceiveRightHandler() = default;

    ReceiveRightHandler(pmos_right_t port, pmos_right_t right)
        : port(port), right(right) {}

    ~ReceiveRightHandler() {
        if (right != INVALID_RIGHT) {
            delete_receive_right(port, right, DELETE_RIGHT_CLEAR_MESSAGE_QUEUE);
        }
    }

    ReceiveRightHandler(ReceiveRightHandler &&other)
        : port(other.port), right(other.right) {
        other.right = INVALID_RIGHT;
    }

    ReceiveRightHandler &operator=(ReceiveRightHandler &&other) {
        if (this != &other) {
            if (right != INVALID_RIGHT) {
                delete_receive_right(port, right, DELETE_RIGHT_CLEAR_MESSAGE_QUEUE);
            }
            port = other.port;
            right = other.right;
            other.right = INVALID_RIGHT;
        }
        return *this;
    }

    ReceiveRightHandler(const ReceiveRightHandler &) = delete;
    ReceiveRightHandler &operator=(const ReceiveRightHandler &) = delete;

    void release() {
        right = INVALID_RIGHT;
    }
};

using RRH = ReceiveRightHandler;

struct Poll_Entry {
    pmos_right_t io_right = INVALID_RIGHT;
    RRH receive_right;
};

static int arm_timer(const struct timespec *timeout, pmos_right_t &out_right) {
    __ensure(timeout);

    auto port = __pmos_prepare_reply_port();
    if (port == INVALID_PORT)
        return EIO;

    auto result = pmos_create_timer(port);
    if (result.result != SUCCESS)
        return kernel_to_errno(result.result);

    uint64_t time = timespec_to_kernel(timeout);

    auto arm_result = pmos_set_timer(port, result.right, time, PMOS_SET_TIMER_RELATIVE);
    if (arm_result) {
        delete_receive_right(port, result.right, DELETE_RIGHT_CLEAR_MESSAGE_QUEUE);
        return arm_result;
    }

    out_right = result.right;
    return 0;
}

int Sysdeps<Ppoll>::operator()(struct pollfd *fds, nfds_t nfds, const struct timespec *timeout, const sigset_t *sigmask, int *num_events) {
    // TODO: Implement signals and sigmask
    (void)sigmask;

    if (nfds > __MLIBC_OPEN_MAX)
        return EINVAL;

    __ensure(num_events);
    *num_events = 0;

    frg::vector<Poll_Entry, MemoryAllocator> poll_entries(getAllocator());
    poll_entries.resize(nfds);

    // 1. Poll file descriptors without blocking. If any events are found, return immediately.
    for (nfds_t i = 0; i < nfds; ++i)
        fds[i].revents = 0;

    size_t ipc_available = 0;

    {
        frg::unique_lock lock(filesystem_mutex);
        for (nfds_t i = 0; i < nfds; ++i) {
            if (fds[i].fd < 0)
                continue;

            if (fds[i].fd >= __MLIBC_OPEN_MAX) {
                fds[i].revents = POLLNVAL;
                (*num_events)++;
                continue;
            }

            auto &file = open_files[fds[i].fd];
            if (file.io_right == INVALID_RIGHT) {
                fds[i].revents = POLLNVAL;
                (*num_events)++;
                continue;
            }

            poll_entries[i].io_right = file.io_right;
            ++ipc_available;
        }
    }

    auto port = __pmos_prepare_reply_port();
    if (port == INVALID_PORT)
        return EIO;

    constexpr uint16_t poll_events_mask = POLLIN | POLLOUT | POLLPRI;

    size_t ipc_pending = 0;

    auto ipc_poll = [&](bool nonblocking) {
        for (nfds_t i = 0; i < nfds; ++i) {
            if (poll_entries[i].io_right == INVALID_RIGHT)
                continue;

            uint16_t flags = 0;
            if (nonblocking)
                flags |= IPC_POLL_FLAG_NONBLOCK;

            IPC_Poll msg = {
                .type = IPC_Poll_NUM,
                .flags = flags,
                .events = static_cast<uint16_t>(fds[i].events & poll_events_mask),
            };

            auto send_result = send_message_right(poll_entries[i].io_right, port, &msg, sizeof(msg), nullptr, 0);
            if (send_result.result != SUCCESS) {
                fds[i].revents |= POLLERR;
                poll_entries[i].io_right = INVALID_RIGHT;
                (*num_events)++;
                continue;
            } else {
                ++ipc_pending;
                poll_entries[i].receive_right = RRH(port, send_result.right);
            }
        }
    };

    auto find_idx = [&](pmos_right_t receive_right) -> unsigned {
        for (unsigned i = 0; i < nfds; ++i) {
            if (poll_entries[i].receive_right.right == receive_right)
                return i;
        }
        __ensure(!"Receive right not found in poll entries");
        return 0;
    };

    auto fd_done = [&](unsigned idx, unsigned events) {
        fds[idx].revents |= events;
        --ipc_pending;
        poll_entries[idx].receive_right.release();
        if (events != 0) {
            poll_entries[idx].io_right = INVALID_RIGHT;
            --ipc_available;
            (*num_events)++;
        }
    };

    ipc_poll(true);
    while (ipc_pending > 0) {
        Message_Descriptor reply_descr;
        auto result = syscall_get_message_info(&reply_descr, port, 0);
        __ensure(result == SUCCESS);
    
        unsigned idx = find_idx(reply_descr.sent_with_right);
        poll_entries[idx].receive_right.release();

        IPC_Poll_Reply reply;
        auto fr = get_first_message(reinterpret_cast<char *>(&reply), MSG_ARG_REJECT_RIGHT, port);
        __ensure(fr.result == SUCCESS);

        if (reply_descr.size < sizeof(IPC_Generic_Msg)) {
            fd_done(idx, POLLERR);
            continue;
        }

        if (reply.type != IPC_Poll_Reply_NUM) {
            fd_done(idx, POLLERR);
            continue;
        }

        if (reply.result_code < 0) {
            fd_done(idx, POLLERR);
            continue;
        }

        fd_done(idx, reply.events);
    }

    if (*num_events > 0)
        return 0;

    RRH timer_receive_right;
    if (timeout) {
        if (timespec_to_kernel(timeout) == 0) {
            return 0;
        }

        int timer_result = arm_timer(timeout, timer_receive_right.right);
        if (timer_result)
            return timer_result;
    }

    // Do the same, but block this time; return when any event occurs, RAII will clean up the receive rights and duplicate messages
    ipc_poll(false);

    if (ipc_pending == 0 && timer_receive_right.right == INVALID_RIGHT) {
        return 0;
    }

    Message_Descriptor reply_descr;
    auto result = syscall_get_message_info(&reply_descr, port, 0);
    __ensure(result == SUCCESS);

    if (reply_descr.sent_with_right == timer_receive_right.right) {
        timer_receive_right.release();
        // Timeout
        return 0;
    }

    unsigned idx = find_idx(reply_descr.sent_with_right);
    IPC_Poll_Reply reply;
    auto fr = get_first_message(reinterpret_cast<char *>(&reply), MSG_ARG_REJECT_RIGHT, port);
    __ensure(fr.result == SUCCESS);

    if (reply_descr.size < sizeof(IPC_Generic_Msg)) {
        fd_done(idx, POLLERR);
        return 0;
    }
    if (reply.type != IPC_Poll_Reply_NUM) {
        fd_done(idx, POLLERR);
        return 0;
    }
    if (reply.result_code < 0) {
        fd_done(idx, POLLERR);
        return 0;
    }

    fd_done(idx, reply.events);
    return 0;
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
