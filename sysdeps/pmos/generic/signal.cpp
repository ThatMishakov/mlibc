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

int Sysdeps<Sigprocmask>::operator()(int how, const sigset_t *__restrict set, sigset_t *__restrict oldset)
{
    auto tcb = mlibc::get_current_tcb();

    if (oldset)
        *oldset = tcb->sysdepData.sigmask;

    // TODO?

    if (set) {
        switch (how) {
        case SIG_BLOCK:
            tcb->sysdepData.sigmask |= *set;
            break;
        case SIG_UNBLOCK:
            tcb->sysdepData.sigmask &= ~(*set);
            break;
        case SIG_SETMASK:
            tcb->sysdepData.sigmask = *set;
            break;
        default:
            return EINVAL;
        }
    }

    return 0;
}

} // namespace mlibc