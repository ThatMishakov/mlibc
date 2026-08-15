#include <bits/ensure.h>
#include <errno.h>
#include <nl_types.h>

#include <mlibc/debug.hpp>

nl_catd catopen(const char *, int ) {
	mlibc::infoLogger() << "mlibc: catopen() is a no-op" << frg::endlog;
	errno = ENOSYS;
	return reinterpret_cast<nl_catd>(-1);
}

int catclose(nl_catd) {
    __ensure(!"Not implemented");
	__builtin_unreachable();
}

char *catgets(nl_catd, int, int, const char *) {
    __ensure(!"Not implemented");
    __builtin_unreachable();
}
