#include <mlibc/all-sysdeps.hpp>

#include <string.h>

#include <pmos/ipc.h>

#include "common.hpp"
#include <frg/mutex.hpp>

namespace {

constinit FutexLock sigaction_mutex;
struct sigaction __sigactions[_NSIG] = {};

}

namespace mlibc {

using namespace pmos;

int Sysdeps<Sigaction>::operator()(int sigval, const struct sigaction *__restrict snew, struct sigaction *__restrict sold)
{
    if (sigval <= 0 || sigval >= _NSIG)
        return EINVAL;

    frg::unique_lock lock(sigaction_mutex);
    if (sold)
        *sold = __sigactions[sigval - 1];

    if (snew)
        __sigactions[sigval - 1] = *snew;

    return 0;
}

} // namespace mlibc