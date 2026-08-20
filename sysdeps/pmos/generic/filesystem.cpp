#include <mlibc/debug.hpp>
#include <mlibc/all-sysdeps.hpp>
#include <mlibc/allocator.hpp>

#include <frg/array.hpp>

#include <pmos/system.h>
#include <mlibc/threads.hpp>
#include <pmos/fs-data.h>

#include <sys/auxv.h>
#include <sys/mman.h>

#define STUB()                                                                                     \
    ({                                                                                             \
        __ensure(!"STUB function was called");                                                     \
        __builtin_unreachable();                                                                   \
})

namespace {

struct RightWrapper {
    pmos_right_t right = INVALID_RIGHT;
    ~RightWrapper() {
        if (right != INVALID_RIGHT)
            delete_right(right);
    }
};

struct OpenFile {
    RightWrapper io_right;
    RightWrapper op_right;
    unsigned flags;
};

struct FileHandle {
    pmos_right_t io_right = INVALID_RIGHT;
    pmos_right_t op_right = INVALID_RIGHT;
    unsigned flags = 0;
};

constexpr unsigned FLAG_ISATTY = 0x01;

constinit FutexLock filesystem_mutex;
// Don't bother freeing this, notably this is needed for ld.so
constinit frg::array<FileHandle, __MLIBC_OPEN_MAX> open_files{};
} // namespace


namespace mlibc {

int Sysdeps<Close>::operator()(int) {
    STUB();
}

int Sysdeps<Write>::operator()(int , const void *, size_t , ssize_t *) {
    STUB();
}

int Sysdeps<Read>::operator()(int , void *, size_t , ssize_t *) {
    STUB();
}

int Sysdeps<Seek>::operator()(int , off_t , int , off_t *) {
    STUB();
}

int Sysdeps<Open>::operator()(const char *, int, mode_t, int *) {
    STUB();
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

void __pmos_fill_fd_table(void *fs_data_ptr)
{
    auto fs_data = (struct FsData *)fs_data_ptr;
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