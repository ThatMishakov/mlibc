#pragma once

#include <mlibc/sysdep-signatures.hpp>

namespace mlibc {

struct PmosSysdepTags :
	LibcPanic,
	LibcLog,
	Isatty,
	Write,
	TcbSet,
	AnonAllocate,
	AnonFree,
	Seek,
	Exit,
	Close,
	FutexWake,
	FutexWait,
	Read,
	Open,
	VmMap,
	VmUnmap,
	ClockGet,
	Recvfrom,
	Dup2,
	Yield
{};

template<typename Tag>
using Sysdeps = SysdepOf<PmosSysdepTags, Tag>;

struct SysdepTraits {
	static constexpr bool usesRtNetlink = false;
};

} // namespace mlibc
