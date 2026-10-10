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
	Yield,
	Sleep,
	PrepareStack,
	Clone,
	ThreadExit,
	Stat,
	Poll,
	Ppoll,
	Sigaction,
	Openpty,
	Openpt,
	Unlockpt,
	Sysconf,
	Ptsname,
	Ttyname,
	Fork,
	SetSid,
	Ioctl,
	Execve,
	GetUid,
	GetEuid,
	GetGid,
	GetEgid,
	GetPid,
	GetPpid,
	Pipe,
	Dup,
	GetPgid,
	SetPgid,
	GetResuid,
	GetResgid,
	Sigprocmask,
	Tcgetattr,
	Pselect,
	Tcgetwinsize,
	Tcsetwinsize,
	Waitpid,
	GetCwd,
	Access,
	Faccessat
{};

template<typename Tag>
using Sysdeps = SysdepOf<PmosSysdepTags, Tag>;

struct SysdepTraits {
	static constexpr bool usesRtNetlink = false;
};

struct PmosTcbData {
	uint64_t threadPort;

	uint64_t sigmask = 0;
};

using SysdepTcbData = PmosTcbData;

} // namespace mlibc
