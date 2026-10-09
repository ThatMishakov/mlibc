#include "common.hpp"
#include <frg/scope_exit.hpp>
#include <frg/mutex.hpp>
#include <frg/string.hpp>
#include <frg/vector.hpp>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <alloca.h>
#include <mlibc/allocator.hpp>
#include <unistd.h>

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
    if (name_length + 1 > size)
        return ERANGE;

    memcpy(buff, reply->tty_name, name_length);
    buff[name_length] = '\0';

    return 0;
}

static int create_new_process(uint64_t child_task_id, pid_t *child_pid, pmos_right_t *child_right) {
    IPC_Register_Process message = {
        .type = IPC_Register_Process_NUM,
        .flags = 0,
    };

    auto port = __pmos_prepare_reply_port();
    if (port == INVALID_PORT)
        return EIO;

    auto process_right = process_for_task(child_task_id, 0);
    if (process_right.result != SUCCESS)
        return kernel_to_errno(process_right.result);
    message_extra_t extra = {
        .extra_rights = {process_right.right},
    };

    auto send_result = send_message_right(__posix_server_right, port, &message, sizeof(message), &extra, 0);
    if (send_result.result != SUCCESS) {
        delete_right(process_right.right);
        return -send_result.result;
    }

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

    IPC_Register_Process_Reply reply;
    result = get_first_message(reinterpret_cast<char *>(&reply), MSG_ARG_REJECT_RIGHT, port).result;
    __ensure(result == SUCCESS);

    if (reply_descr.size < sizeof(IPC_Generic_Msg))
        return EIO;

    if (reply.type != IPC_Register_Process_Reply_NUM)
        return EIO;

    if (reply.result < 0)
        return -reply.result;

    *child_pid = reply.pid;
    *child_right = extra_rights[0];
    if (*child_right == INVALID_RIGHT)
        return EIO;
    
    extra_rights[0] = INVALID_RIGHT;

    return 0;
}

static int copy_open_files(frg::array<OpenFile, __MLIBC_OPEN_MAX> &open_files_copy, uint64_t task_group_id) {
    frg::unique_lock lock(filesystem_mutex);
    for (unsigned i = 0; i < __MLIBC_OPEN_MAX; ++i) {
        if (open_files[i].io_right != INVALID_RIGHT) {
            pmos_right_t io_right = -1, op_right = -1;
            
            auto result = dup_right(open_files[i].op_right);
            if (result.result != SUCCESS)
                return kernel_to_errno(result.result);
            
            auto transfer_result = transfer_right(task_group_id, result.right, 0);
            if (transfer_result.result != SUCCESS) {
                delete_right(result.right);
                return kernel_to_errno(transfer_result.result);
            }
            op_right = transfer_result.right;

            result = dup_right(open_files[i].io_right);
            if (result.result != SUCCESS)
                return kernel_to_errno(result.result);
            transfer_result = transfer_right(task_group_id, result.right, 0);
            if (transfer_result.result != SUCCESS) {
                delete_right(result.right);
                return kernel_to_errno(transfer_result.result);
            }
            io_right = transfer_result.right;


            open_files_copy[i].io_right = io_right;
            open_files_copy[i].op_right = op_right;
            open_files_copy[i].flags = open_files[i].flags;
        }
    }

    return 0;
}

int Sysdeps<Fork>::operator()(pid_t *child_pid) {
    pmos_right_t child_right;
    syscall_r r = syscall_new_task(PROCESS_RIGHT_NEW);
    if (r.result != SUCCESS)
        return kernel_to_errno(r.result);
    // TODO: Make this return the right...

    auto result = create_new_process(r.value, child_pid, &child_right);
    if (result)
        return result;
    frg::scope_exit delete_child_right([&] {
        delete_right(child_right);
    });

    syscall_r rr = create_task_group();
    if (rr.result != SUCCESS)
        return kernel_to_errno(rr.result);
    frg::scope_exit delete_task_group([&] {
        remove_task_from_group(TASK_ID_SELF, rr.value);
    });

    auto add_result = add_task_to_group(r.value, rr.value, 0).result;
    if (add_result != SUCCESS)
        return kernel_to_errno(add_result);

    auto transfer_result = transfer_right(rr.value, child_right, 0);
    if (transfer_result.result != SUCCESS)
        return kernel_to_errno(transfer_result.result);
    pmos_right_t new_posix_right = transfer_result.right;
    delete_child_right.release();

    frg::array<OpenFile, __MLIBC_OPEN_MAX> open_files_copy{};
    int copy_result = copy_open_files(open_files_copy, rr.value);
    if (copy_result)
        return copy_result;

    auto clone_result = pmos_clone(r.value);
    if (clone_result.result != SUCCESS)
        return kernel_to_errno(clone_result.result);

    if (clone_result.value == 0) {
        delete_task_group.release();

        // Child process
        __posix_server_right = new_posix_right;
        __process_task_group = rr.value;
        open_files = std::move(open_files_copy);
        *child_pid = 0;
        auto set_result = set_namespace(__process_task_group, NAMESPACE_RIGHTS);
        __ensure(set_result.result == SUCCESS);

        auto tcb = mlibc::get_current_tcb();
        tcb->sysdepData.threadPort = INVALID_PORT;

        return 0;
    } else {
        return 0;
    }
}

int Sysdeps<SetSid>::operator()(pid_t *out) {
    auto port = __pmos_prepare_reply_port();
    if (port == INVALID_PORT)
        return EIO;

    IPC_Setsid message = {
        .type = IPC_Setsid_NUM,
        .flags = 0,
    };
    auto send_result = send_message_right(__posix_server_right, port, &message, sizeof(message), nullptr, 0);
    if (send_result.result != SUCCESS)
        return -send_result.result;

    Message_Descriptor reply_descr;
    auto result = syscall_get_message_info(&reply_descr, port, 0);
    __ensure(result == SUCCESS);

    IPC_Setid_Reply reply;
    result = get_first_message(reinterpret_cast<char *>(&reply), MSG_ARG_REJECT_RIGHT, port).result;
    __ensure(result == SUCCESS);

    if (reply_descr.size < sizeof(IPC_Generic_Msg))
        return EIO;

    if (reply.type != IPC_Setid_Reply_NUM)
        return EIO;

    if (reply.result_id < 0)
        return -reply.result_id;

    *out = reply.result_id;
    return 0;
}

int Sysdeps<SetPgid>::operator()(pid_t pid, pid_t pgid) {
    auto port = __pmos_prepare_reply_port();
    if (port == INVALID_PORT)
        return EIO;

    IPC_Setpgid message = {
        .type = IPC_Setpgid_NUM,
        .flags = 0,
        .pid = pid,
        .pgid = pgid,
    };
    auto send_result = send_message_right(__posix_server_right, port, &message, sizeof(message), nullptr, 0);
    if (send_result.result != SUCCESS)
        return -send_result.result;

    Message_Descriptor reply_descr;
    auto result = syscall_get_message_info(&reply_descr, port, 0);
    __ensure(result == SUCCESS);

    IPC_Setid_Reply reply;
    result = get_first_message(reinterpret_cast<char *>(&reply), MSG_ARG_REJECT_RIGHT, port).result;
    __ensure(result == SUCCESS);

    if (reply_descr.size < sizeof(IPC_Generic_Msg))
        return EIO;

    if (reply.type != IPC_Setid_Reply_NUM)
        return EIO;

    if (reply.result_id < 0)
        return -reply.result_id;

    return 0;
}

struct OpenFileIpc {
    pmos_right_t io_right;
    pmos_right_t op_right;
    uint64_t flags;
};

static int prepare_filesystem_object(pmos_right_t &new_fs_right)
{
    size_t page_size = getpagesize();
    auto page_mask = page_size - 1;

    auto memory_size = sizeof(OpenFileIpc) * __MLIBC_OPEN_MAX;
    auto size_aligned = (memory_size + page_mask) & ~page_mask;

    right_request_t object = create_mem_object(size_aligned, 0);
    if (object.result != SUCCESS)
        return kernel_to_errno(object.result);
    frg::scope_exit delete_object([&] {
        delete_right(object.right);
    });
    
    map_mem_object_param_t map_params = {
        .page_table_id = 0,
        .object_right = object.right,
        .addr_start_uint = 0,
        .size = size_aligned,
        .offset_object = 0,
        .object_size = size_aligned,
        .access_flags = PROT_READ | PROT_WRITE,
    };

    auto map_result = map_mem_object(&map_params);
    if (map_result.result != SUCCESS)
        return kernel_to_errno(map_result.result);
    frg::scope_exit unmap_object([&] {
        Sysdeps<VmUnmap>()(map_result.virt_addr, size_aligned);
    });

    frg::unique_lock lock(filesystem_mutex);
    auto *open_files_ipc = reinterpret_cast<OpenFileIpc *>(map_result.virt_addr);
    for (unsigned i = 0; i < __MLIBC_OPEN_MAX; ++i) {
        open_files_ipc[i].io_right = open_files[i].io_right;
        open_files_ipc[i].op_right = open_files[i].op_right;
        open_files_ipc[i].flags = open_files[i].flags;
    }

    delete_object.release();
    new_fs_right = object.right;
    return 0;
}

int Sysdeps<Execve>::operator()(const char *path, char *const argv[], char *const envp[]) {
	frg::string<MemoryAllocator> args_area(getAllocator());
	for (auto it = argv; *it; ++it)
		args_area += frg::string_view{*it, strlen(*it) + 1};

	frg::string<MemoryAllocator> env_area(getAllocator());
	for (auto it = envp; *it; ++it)
		env_area += frg::string_view{*it, strlen(*it) + 1};

    IPC_Execve message = {
        .type = IPC_Execve_NUM,
        .flags = 0,
        .path_length = strlen(path) + 1,
        .args_length = args_area.size(),
        .envs_length = env_area.size(),
        .data = {},
    };

    frg::vector<char, MemoryAllocator> message_data(getAllocator());
    message_data.resize(sizeof(message) + message.path_length + message.args_length + message.envs_length);
    auto ptr = message_data.data();
    memcpy(ptr, &message, sizeof(message));
    ptr += sizeof(message);
    memcpy(ptr, path, message.path_length);
    ptr += message.path_length;
    memcpy(ptr, args_area.data(), message.args_length);
    ptr += message.args_length;
    memcpy(ptr, env_area.data(), message.envs_length);

    auto port = __pmos_prepare_reply_port();
    if (port == INVALID_PORT)
        return EIO;

    pmos_right_t fs_right;
    int prepare_result = prepare_filesystem_object(fs_right);
    if (prepare_result)
        return prepare_result;

    right_request_t task_group_right = right_for_task_group(__process_task_group);
    if (task_group_right.result != SUCCESS) {
        delete_right(fs_right);
        return kernel_to_errno(task_group_right.result);
    }

    message_extra_t extra = {
        .extra_rights = {fs_right, task_group_right.right},
    };

    auto send_result = send_message_right(__posix_server_right, port, message_data.data(), message_data.size(), &extra, 0);
    if (send_result.result != SUCCESS) {
        delete_right(task_group_right.right);
        delete_right(fs_right);
        return -send_result.result;
    }

    Message_Descriptor reply_descr;
    auto result = syscall_get_message_info(&reply_descr, port, 0);
    __ensure(result == SUCCESS);

    IPC_Execve_Reply reply;
    result = get_first_message(reinterpret_cast<char *>(&reply), MSG_ARG_REJECT_RIGHT, port).result;
    __ensure(result == SUCCESS);

    if (reply_descr.size < sizeof(IPC_Generic_Msg))
        return EIO;

    if (reply.type != IPC_Execve_Reply_NUM)
        return EIO;

    return -reply.result_code;
}

static uint32_t get_id_generic(uint16_t id_type) {
    IPC_Get_ID message = {
        .type = IPC_Get_ID_NUM,
        .flags = 0,
        .id_type = id_type,
    };

    auto port = __pmos_prepare_reply_port();
    __ensure(port != INVALID_PORT);

    auto send_result = send_message_right(__posix_server_right, port, &message, sizeof(message), nullptr, 0);
    __ensure(send_result.result == SUCCESS);

    Message_Descriptor reply_descr;
    auto result = syscall_get_message_info(&reply_descr, port, 0);
    __ensure(result == SUCCESS);

    IPC_Get_ID_Reply reply;
    result = get_first_message(reinterpret_cast<char *>(&reply), MSG_ARG_REJECT_RIGHT, port).result;
    __ensure(result == SUCCESS);

    __ensure(reply_descr.size >= sizeof(IPC_Generic_Msg));
    __ensure(reply.type == IPC_Get_ID_Reply_NUM);

    __ensure(reply.result == 0);
    return reply.id;
}

uid_t Sysdeps<GetUid>::operator()() {
    return get_id_generic(IPC_GET_ID_TYPE_UID);
}
uid_t Sysdeps<GetEuid>::operator()() {
    return get_id_generic(IPC_GET_ID_TYPE_EUID);
}
uid_t Sysdeps<GetGid>::operator()() {
    return get_id_generic(IPC_GET_ID_TYPE_GID);
}
uid_t Sysdeps<GetEgid>::operator()() {
    return get_id_generic(IPC_GET_ID_TYPE_EGID);
}
pid_t Sysdeps<GetPid>::operator()() {
    return get_id_generic(IPC_GET_ID_TYPE_PID);
}
pid_t Sysdeps<GetPpid>::operator()() {
    return get_id_generic(IPC_GET_ID_TYPE_PPID);
}

int Sysdeps<Pipe>::operator()(int *fds, int flags) {
    IPC_Pipe_Open message = {
        .type           = IPC_Pipe_Open_NUM,
        .flags          = static_cast<uint32_t>(flags),
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

    IPC_Pipe_Open_Reply reply;
    result = get_first_message(reinterpret_cast<char *>(&reply), MSG_ARG_REJECT_RIGHT, port).result;
    __ensure(result == SUCCESS);

    if (reply_descr.size < sizeof(IPC_Generic_Msg))
        return EIO;

    if (reply.type != IPC_Pipe_Open_Reply_NUM)
        return EIO;

    if (reply.result_code < 0)
        return -reply.result_code;

    fds[0] = -1;
    fds[1] = -1;

    frg::unique_lock lock(filesystem_mutex);
    for (unsigned i = 0; i < __MLIBC_OPEN_MAX; ++i) {
        if (open_files[i].io_right == INVALID_RIGHT) {
            open_files[i].io_right = extra_rights[0];
            open_files[i].op_right = extra_rights[0];
            open_files[i].flags    = reply.flags;
            fds[0] = i;

            break;
        }
    }

    if (fds[0] == -1)
        return EMFILE;

    for (unsigned i = 0; i < __MLIBC_OPEN_MAX; ++i) {
        if (open_files[i].io_right == INVALID_RIGHT) {
            open_files[i].io_right = extra_rights[1];
            open_files[i].op_right = extra_rights[1];
            open_files[i].flags    = reply.flags;

            fds[1] = i;

            break;
        }
    }

    if (fds[1] == -1) {
        open_files[fds[0]].io_right = INVALID_RIGHT;
        open_files[fds[0]].op_right = INVALID_RIGHT;
        return EMFILE;
    }

    extra_rights[0] = INVALID_RIGHT;
    extra_rights[1] = INVALID_RIGHT;
    return 0;
}

int Sysdeps<Dup>::operator()(int fd, int flags, int *newfd) {
    if (fd >= __MLIBC_OPEN_MAX || fd < 0)
        return EBADF;

    frg::unique_lock lock(filesystem_mutex);
    if (open_files[fd].io_right == INVALID_RIGHT)
        return EBADF;

    for (unsigned i = 0; i < __MLIBC_OPEN_MAX; ++i) {
        if (open_files[i].io_right == INVALID_RIGHT) {
            auto result = dup_right(open_files[fd].io_right);
            if (result.result != SUCCESS)
                return kernel_to_errno(result.result);

            auto op_result = dup_right(open_files[fd].op_right);
            if (op_result.result != SUCCESS) {
                delete_right(result.right);
                return kernel_to_errno(op_result.result);
            }

            unsigned flags_norm = flags & FD_CLOEXEC ? O_CLOEXEC : 0;

            open_files[i].io_right = result.right;
            open_files[i].op_right = op_result.right;
            open_files[i].flags    = open_files[fd].flags & (~O_CLOEXEC);
            open_files[i].flags   |= flags_norm;

            *newfd = i;
            return 0;
        }
    }

    return EMFILE;
}

static int get_id_for_generic(uint16_t id_type, pid_t pid, pid_t *id) {
    IPC_Get_ID_For message = {
        .type = IPC_Get_ID_For_NUM,
        .flags = 0,
        .id_type = id_type,
        .pid = pid,
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

    IPC_Get_ID_Reply reply;
    result = get_first_message(reinterpret_cast<char *>(&reply), MSG_ARG_REJECT_RIGHT, port).result;
    __ensure(result == SUCCESS);

    if (reply_descr.size < sizeof(IPC_Generic_Msg))
        return EIO;

    if (reply.type != IPC_Get_ID_Reply_NUM)
        return EIO;

    if (reply.result < 0)
        return -reply.result;

    *id = reply.id;
    return 0;
}

int Sysdeps<GetPgid>::operator()(pid_t pid, pid_t *pgid) {
    return get_id_for_generic(IPC_GET_ID_FOR_TYPE_PGID, pid, pgid);
}

extern "C" int get_fd_rights(int fd, pmos_right_t *op_right, pmos_right_t *io_right, unsigned *flags) {
    if (fd >= __MLIBC_OPEN_MAX || fd < 0)
        return EBADF;

    frg::unique_lock lock(filesystem_mutex);
    if (open_files[fd].io_right == INVALID_RIGHT)
        return EBADF;

    if (op_right)
        *op_right = open_files[fd].op_right;
    if (io_right)
        *io_right = open_files[fd].io_right;
    if (flags)
        *flags = open_files[fd].flags;

    return 0;
}

int Sysdeps<Tcgetwinsize>::operator()(int fd, struct winsize *winsz) {
	int result;
	return sysdep<Ioctl>(fd, TIOCGWINSZ, winsz, &result);
}

int Sysdeps<Tcsetwinsize>::operator()(int fd, const struct winsize *winsz) {
	int result;
	return sysdep<Ioctl>(fd, TIOCSWINSZ, const_cast<struct winsize *>(winsz), &result);
}

int Sysdeps<Tcgetattr>::operator()(int fd, struct termios *attr){
    if (fd >= __MLIBC_OPEN_MAX || fd < 0)
        return EBADF;

    if (!isatty(fd))
        return ENOTTY;

    uint64_t io_right;
    {
        frg::unique_lock lock(filesystem_mutex);
        if (open_files[fd].io_right == INVALID_RIGHT)
            return EBADF;

        io_right = open_files[fd].io_right;
    }

    IPC_Tcgetattr message = {
        .type = IPC_Tcgetattr_NUM,
        .flags = 0,
    };

    auto port = __pmos_prepare_reply_port();
    if (port == INVALID_PORT)
        return EIO;

    auto send_result = send_message_right(io_right, port, &message, sizeof(message), nullptr, 0);
    if (send_result.result != SUCCESS)
        return -send_result.result;

    Message_Descriptor reply_descr;
    auto result = syscall_get_message_info(&reply_descr, port, 0);
    __ensure(result == SUCCESS);

    IPC_Tcgetattr_Reply reply;
    result = get_first_message(reinterpret_cast<char *>(&reply), MSG_ARG_REJECT_RIGHT, port).result;
    __ensure(result == SUCCESS);

    if (reply_descr.size < sizeof(IPC_Generic_Msg))
        return EIO;

    if (reply.type != IPC_Tcgetattr_Reply_NUM)
        return EIO;

    if (reply.result_code < 0)
        return -reply.result_code;

    memcpy(attr, &reply.termios, sizeof(struct termios));
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

int Sysdeps<Pselect>::operator()(int num_fds, fd_set *read_set, fd_set *write_set, fd_set *except_set, const struct timespec *timeout, const sigset_t *sigmask, int *num_events) {
    pollfd *fds = (pollfd *)malloc(num_fds * sizeof(pollfd));

    if(fds == NULL)
        return ENOMEM;

    int actual_count = 0;

    for(int fd = 0; fd < num_fds; ++fd) {
        short events = 0;
        if(read_set && FD_ISSET(fd, read_set)) {
            events |= POLLIN;
        }

        if(write_set && FD_ISSET(fd, write_set)) {
            events |= POLLOUT;
        }

        if(except_set && FD_ISSET(fd, except_set)) {
            events |= POLLPRI;
        }

        if(events) {
            fds[actual_count].fd = fd;
            fds[actual_count].events = events;
            fds[actual_count].revents = 0;
            actual_count++;
        }
    }

    int num;
    int err = sysdep<Ppoll>(fds, actual_count, timeout, sigmask, &num);

    if(err) {
        free(fds);
        return err;
    }

    #define READ_SET_POLLSTUFF (POLLIN | POLLHUP | POLLERR)
    #define WRITE_SET_POLLSTUFF (POLLOUT | POLLERR)
    #define EXCEPT_SET_POLLSTUFF (POLLPRI)

    int return_count = 0;
    for(int fd = 0; fd < actual_count; ++fd) {
        int events = fds[fd].events;
        if((events & POLLIN) && (fds[fd].revents & READ_SET_POLLSTUFF) == 0) {
            FD_CLR(fds[fd].fd, read_set);
            events &= ~POLLIN;
        }

        if((events & POLLOUT) && (fds[fd].revents & WRITE_SET_POLLSTUFF) == 0) {
            FD_CLR(fds[fd].fd, write_set);
            events &= ~POLLOUT;
        }

        if((events & POLLPRI) && (fds[fd].revents & EXCEPT_SET_POLLSTUFF) == 0) {
            FD_CLR(fds[fd].fd, except_set);
            events &= ~POLLPRI;
        }

        if(events)
            return_count++;
    }
    *num_events = return_count;
    free(fds);
    return 0;
}

int Sysdeps<Waitpid>::operator()(pid_t pid, int *status, int flags, struct rusage *ru, pid_t *ret_pid) {
    IPC_Waitpid message = {
        .type = IPC_Waitpid_NUM,
        .flags = static_cast<uint32_t>(flags),
        .pid = pid,
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

    IPC_Waitpid_Reply reply;
    result = get_first_message(reinterpret_cast<char *>(&reply), MSG_ARG_REJECT_RIGHT, port).result;
    __ensure(result == SUCCESS);

    if (reply_descr.size < sizeof(IPC_Generic_Msg))
        return EIO;

    if (reply.type != IPC_Waitpid_Reply_NUM)
        return EIO;

    if (reply.result_code < 0)
        return -reply.result_code;

    *ret_pid = reply.child_pid;
    if (status)
        *status = reply.status;
    if (ru)
        *ru = reply.rusage;
    
    return 0;
}

} // namespace mlibc