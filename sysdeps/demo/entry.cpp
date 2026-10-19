#include <mlibc/all-sysdeps.hpp>

// MLIBC_INIT_PRIORITY_ENTRY definition
#include <mlibc/init-priority.hpp>

namespace mlibc {

void Sysdeps<PreMainHook>::operator()() {
	// In case this is needed, a libc initialization code placed here will
	// be run by mlibc while initializing the program, after running all of the constructors, but before
	// entering the main() function.
}

} // namespace mlibc

[[gnu::constructor(MLIBC_INIT_PRIORITY_ENTRY)]]
static void run_before_constructors() {
	// This code here will be run by mlibc before enetering the main() function,
	// and before running any of the constructors. Note that C++ can ommit the
	// constructors in files that do not have any global objects that are referenced
	// by the other parts of the program, so this needs to be in a file that does that,
	// for example the one that defines Sysdeps<PreMainHook>, since that is always called,
	// even if empty.
}
