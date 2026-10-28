#include <mlibc/debug.hpp>
#include <mlibc/all-sysdeps.hpp>

#include <frg/array.hpp>
#include <frg/scope_exit.hpp>
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
#include <fcntl.h>

#include <utility>

#include "common.hpp"

#define STUB()                                                                                     \
    ({                                                                                             \
        __ensure(!"STUB function was called");                                                     \
        __builtin_unreachable();                                                                   \
})

namespace mlibc::pmos {

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
    auto res = mlibc::pmos::get_open_file(fd);
    if (!res)
        return res.error();

    pmos_right_t io_right = res.value().io_right;
    if (io_right == INVALID_RIGHT)
        return EBADF;

    auto port = mlibc::pmos::prepare_reply_port();
    if (port == INVALID_PORT)
        return EIO;

    IPC_Write *write_msg = (IPC_Write *)alloca(sizeof(IPC_Write) + count);
    write_msg->type = IPC_Write_NUM;
    write_msg->flags = flags_to_io(res.value().flags);
    write_msg->offset = 0;
    memcpy(write_msg->data, buff, count);

    pmos_right_t reply_right;
    do {
        auto send_result = send_message_right(io_right, port, write_msg, sizeof(IPC_Write) + count, nullptr, 0);
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
        write_msg->flags = flags_to_io(res.value().flags);
    } while (true);

    IPC_Write_Reply reply;
    size_t bytes_read;
    auto span = frg::span<uint8_t>((uint8_t *)&reply, sizeof(reply));
    if (auto result = mlibc::pmos::handle_get_message_right(port, reply_right, span, bytes_read, false); !result)
        return result.error();

    if (auto result = mlibc::pmos::check_generic_errors(span.subspan(0, bytes_read)); !result)
        return result.error();

    if (reply.type != IPC_Write_Reply_NUM)
        return EIO;

    *bytes_written = static_cast<ssize_t>(reply.bytes_written);
    return -reply.result_code;
}

int Sysdeps<Read>::operator()(int fd, void *buff, size_t count, ssize_t *bytes_read) {
    auto res = mlibc::pmos::get_open_file(fd);
    if (!res)
        return res.error();

    pmos_right_t io_right = res.value().io_right;
    if (io_right == INVALID_RIGHT)
        return EBADF;

    auto port = mlibc::pmos::prepare_reply_port();
    if (port == INVALID_PORT)
        return EIO;

    IPC_Read read_msg = {
        .type = IPC_Read_NUM,
        .flags = flags_to_io(res.value().flags),
        .start_offset = 0, // This is ignored since there's no fixed offset flag set
        .max_size = count,
    };

    pmos_right_t reply_right;
    do {
        auto send_result = send_message_right(io_right, port, &read_msg, sizeof(read_msg), nullptr, 0);
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
        read_msg.flags = flags_to_io(res.value().flags);
    } while (true);

    frg::vector<uint8_t, SysdepsAllocator> reply_data(getSysdepsAllocator());
    if (auto result = mlibc::pmos::handle_get_message_right(port, reply_right, reply_data, false); !result)
        return result.error();

    if (auto result = mlibc::pmos::check_generic_errors({reply_data.begin(), reply_data.size()}); !result)
        return result.error();

    IPC_Read_Reply *reply = (IPC_Read_Reply *)reply_data.data();

    if (reply->type != IPC_Read_Reply_NUM)
        return EIO;

    if (reply->result_code)
        return -reply->result_code;

    *bytes_read = reply_data.size() - sizeof(IPC_Read_Reply);
    memcpy(buff, reply->data, *bytes_read);
    return 0;
}

int Sysdeps<Seek>::operator()(int fd, off_t offset, int whence, off_t *new_offset) {
    auto res = mlibc::pmos::get_open_file(fd);
    if (!res)
        return res.error();

    pmos_right_t io_right = res.value().io_right;
    auto flags = res.value().flags;
    if (io_right == INVALID_RIGHT)
        return EBADF;

    if (io_right == INVALID_RIGHT)
        return EBADF;

    if (flags & (FLAG_ISPIPE | FLAG_ISATTY))
        return ESPIPE;

    auto port = mlibc::pmos::prepare_reply_port();
    if (port == INVALID_PORT)
        return EIO;

    IPC_Seek seek_msg = {
        .type = IPC_Seek_NUM,
        .flags = 0,
        .whence = static_cast<uint16_t>(whence),
        .offset = offset,
    };
    
    pmos_right_t reply_right;
    do {
        auto send_result = send_message_right(io_right, port, &seek_msg, sizeof(seek_msg), nullptr, 0);
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
    } while (true);

    IPC_Seek_Reply reply;
    size_t bytes_read;
    if (auto result = mlibc::pmos::handle_get_message_right(port, reply_right, frg::span<uint8_t>((uint8_t *)&reply, sizeof(reply)), bytes_read, true); !result)
        return result.error();

    if (auto result = mlibc::pmos::check_generic_errors(frg::span<uint8_t>((uint8_t *)&reply, bytes_read)); !result)
        return result.error();

    if (reply.type != IPC_Seek_Reply_NUM)
        return EIO;

    if (bytes_read < sizeof(reply))
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

    auto port = mlibc::pmos::prepare_reply_port();
    if (port == INVALID_PORT)
        return EIO;

    auto send_result = send_message_right(__posix_server_right, port, message, message_size, nullptr, 0);
    if (send_result.result != SUCCESS)
        return -send_result.result;


    pmos_right_t extra_rights[4] = {};
    frg::scope_exit delete_rights([&] {
    for (size_t i = 0; i < 4; ++i) {
            if (extra_rights[i] != INVALID_RIGHT) {
                delete_right(extra_rights[i]);
            }
        }
    });

    IPC_Open_Reply reply;
    size_t bytes_read;
    if (auto result = mlibc::pmos::handle_get_message_right(port, send_result.right, frg::span<uint8_t>((uint8_t *)&reply, sizeof(reply)), bytes_read, false, &extra_rights); !result)
        return result.error();

    if (auto result = mlibc::pmos::check_generic_errors(frg::span<uint8_t>((uint8_t *)&reply, bytes_read)); !result)
        return result.error();

    if (bytes_read < sizeof(IPC_Open_Reply))
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
            open_files[i].flags    = flags | reply.fs_flags;
            *fd = i;

            extra_rights[0] = INVALID_RIGHT;
            extra_rights[1] = INVALID_RIGHT;

            return 0;
        }
    }

    return EMFILE;
}

int Sysdeps<Isatty>::operator()(int fd) {
    auto file = mlibc::pmos::get_open_file(fd);
    if (!file)
        return file.error();

    if (file.value().io_right == INVALID_RIGHT)
        return EBADF;

    if (!(file.value().flags & FLAG_ISATTY))
        return ENOTTY;

    return 0;
}

int Sysdeps<Recvfrom>::operator()(int , void *, size_t , int , struct sockaddr *, socklen_t *, ssize_t *) {
    STUB();
}

int Sysdeps<Dup2>::operator()(int fd, int flags, int newfd) {
    if (fd >= __MLIBC_OPEN_MAX || fd < 0)
        return EBADF;
    if (newfd >= __MLIBC_OPEN_MAX || newfd < 0)
        return EBADF;

    frg::unique_lock lock(filesystem_mutex);

    pmos_right_t io_right, op_right;
    unsigned file_flags;
    {
        io_right = open_files[fd].io_right;
        op_right = open_files[fd].op_right;
        file_flags = open_files[fd].flags;
    }

    if (io_right == INVALID_RIGHT)
        return EBADF;

    auto io_dup_result = dup_right(io_right);
    if (io_dup_result.result != SUCCESS)
        return kernel_to_errno(io_dup_result.result);
    auto op_dup_result = dup_right(op_right);
    if (op_dup_result.result != SUCCESS) {
        delete_right(io_dup_result.right);
        return kernel_to_errno(op_dup_result.result);
    }

    if (open_files[newfd].io_right != INVALID_RIGHT) {
        delete_right(open_files[newfd].io_right);
        delete_right(open_files[newfd].op_right);
    }

    unsigned flags_norm = flags & FD_CLOEXEC ? O_CLOEXEC : 0;

    open_files[newfd].io_right = io_dup_result.right;
    open_files[newfd].op_right = op_dup_result.right;
    open_files[newfd].flags = file_flags & (~O_CLOEXEC);
    open_files[newfd].flags |= flags_norm;

    return 0;
    
}

int Sysdeps<VmMap>::operator()(void *hint, size_t size, int prot, int flags, int fd, off_t offset, void **window) {
    // TODO: Signal safety here
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

    auto port = mlibc::pmos::prepare_reply_port();
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
    auto get_result = accept_rights(port, reply_descr.message_id, extra_rights);
    __ensure(get_result == SUCCESS);
    frg::scope_exit delete_rights([&] {
        for (size_t i = 0; i < 4; ++i) {
            if (extra_rights[i] != INVALID_RIGHT) {
                delete_right(extra_rights[i]);
            }
        }
    });

    IPC_Get_Object_Reply reply;
    result = read_message(reinterpret_cast<char *>(&reply), sizeof(reply), MSG_ARG_REJECT_RIGHT, port, reply_descr.message_id).result;
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

    auto stat_right = [=]() -> frg::expected<int, std::pair<pmos_right_t, unsigned /* flags */>> {
        unsigned flags = 0;
        switch (fsfdt) {
            case fsfd_target::path:
                return std::pair<pmos_right_t, unsigned>(__posix_server_right, 0);
            case fsfd_target::fd:
                flags = AT_EMPTY_PATH;
                [[fallthrough]];

            case fsfd_target::fd_path: {
                auto res = mlibc::pmos::get_open_file(fd);
                if (!res)
                    return res.error();

                return std::pair<pmos_right_t, unsigned>(res.value().op_right, flags);
            }
            default:
                return ENOSYS;
        }
    };

    auto port = mlibc::pmos::prepare_reply_port();
    if (port == INVALID_PORT)
        return EIO;

    size_t path_len = strlen(path);
    size_t message_size = sizeof(IPC_Stat) + path_len;
    IPC_Stat *message = (IPC_Stat *)alloca(message_size);
    message->type = IPC_Stat_NUM;
    message->flags = flags;
    memcpy(message->path, path, path_len);

    auto right_result = stat_right();
    if (!right_result)
        return right_result.error();
    if (right_result.value().first == INVALID_RIGHT)
        return EBADF;

    pmos_right_t reply_right;
    do {
        auto send_result = send_message_right(right_result.value().first, port, message, message_size, nullptr, 0);
        auto result = kernel_to_errno(send_result.result);
        reply_right = send_result.right;
        if (!result)
            break;

        if (result != ENOENT)
            return result;

        auto res = stat_right();
        __ensure(res);

        if (res.value().first == right_result.value().first || res.value().first == INVALID_RIGHT)
            return EBADF;

        right_result = std::move(res);
        message->flags = right_result.value().second;
    } while (true);

    IPC_Stat_Reply reply;
    size_t bytes_read;
    auto span = frg::span<uint8_t>((uint8_t *)&reply, sizeof(reply));
    if (auto result = mlibc::pmos::handle_get_message_right(port, reply_right, span, bytes_read, true); !result)
        return result.error();
    if (auto result = mlibc::pmos::check_generic_errors(span.subspan(0, bytes_read)); !result)
        return result.error();

    if (reply.type != IPC_Stat_Reply_NUM)
        return EIO;

    if (bytes_read < sizeof(IPC_Stat_Reply))
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

}

pmos_port_t mlibc::pmos::prepare_reply_port()
{
    auto tcb = mlibc::get_current_tcb();
    auto &port = tcb->sysdepData.threadPort;

    // TODO: Signal safety here
    if (port != INVALID_PORT)
        return port;

    ports_request_t port_request = create_port(TASK_ID_SELF, 0);
    __ensure(port_request.result == SUCCESS);
    
    return port = port_request.port;
}

frg::expected<int, mlibc::OpenFile> mlibc::pmos::get_open_file(int fd)
{
    // TODO: Signal safety here
    if (fd >= __MLIBC_OPEN_MAX || fd < 0)
        return EBADF;

    frg::unique_lock lock(mlibc::filesystem_mutex);
    return mlibc::open_files[fd];
}

frg::expected<int, void> mlibc::pmos::check_generic_errors(frg::span<uint8_t> reply_data, bool check_closed_right) {
    if (reply_data.size() < sizeof(IPC_Generic_Msg))
        return EIO;

    if (reply_data.size() >= sizeof(IPC_Generic_Result)) {
        auto result = (IPC_Generic_Result *)reply_data.data();
        if (result->type == IPC_Generic_Result_NUM && result->result < 0)
            return -result->result;
    }

    if (reply_data.size() >= sizeof(IPC_Kernel_Receive_Right_Destroyed)) {
        auto msg = (IPC_Kernel_Receive_Right_Destroyed *)reply_data.data();
        if (check_closed_right && msg->type == IPC_Kernel_Receive_Right_Destroyed_NUM)
            return ECONNRESET;
    }

    return {};
}

namespace {

frg::expected<int> handle_get_message_generic(pmos_port_t port, pmos_right_t right, auto &container, size_t &bytes_written, bool always_restart, pmos_right_t (*extra_rights)[4]) {
    Message_Descriptor reply_descr;
    do {
        auto result = syscall_get_message_info(&reply_descr, port, 0);
        if (result == -EINTR) {
            if (always_restart)
                continue;

            auto result = delete_receive_right(port, right, 0);
            if (result == -ENOMSG)
                // Message was already recieved
                continue;

            if (result == SUCCESS)
                return -EINTR;

            return kernel_to_errno(result);
        } else if (result != SUCCESS)
            return kernel_to_errno(result);

        break;
    } while (true);

    if constexpr (std::is_same_v<frg::vector<uint8_t, SysdepsAllocator>, std::remove_cvref_t<decltype(container)>>)
        if (container.size() != reply_descr.size)
            container.resize(reply_descr.size);

    if (extra_rights) {
        auto get_result = accept_rights(port, reply_descr.message_id, *extra_rights);
        __ensure(get_result == SUCCESS);
    }

    auto get_result = read_message((char *)container.data(), container.size(), MSG_ARG_REJECT_RIGHT, port, reply_descr.message_id);
    __ensure(get_result.result == SUCCESS);

    bytes_written = std::min<size_t>(reply_descr.size, container.size());

    if (get_result.result != SUCCESS)
        return kernel_to_errno(get_result.result);
    return frg::success;
}

}

frg::expected<int, void> mlibc::pmos::handle_get_message_right(pmos_port_t port, pmos_right_t right, frg::vector<uint8_t, SysdepsAllocator> &out_buffer, bool always_restart, pmos_right_t (*extra_rights)[4]) {
    size_t bytes_read;
    return handle_get_message_generic(port, right, out_buffer, bytes_read, always_restart, extra_rights);
}

frg::expected<int, void> mlibc::pmos::handle_get_message_right(pmos_port_t port, pmos_right_t right, frg::span<uint8_t> out_buffer, size_t &bytes_read, bool always_restart, pmos_right_t (*extra_rights)[4]) {
    return handle_get_message_generic(port, right, out_buffer, bytes_read, always_restart, extra_rights);
}