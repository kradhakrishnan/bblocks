#pragma once

#include <inttypes.h>
#include <sys/resource.h>
#include <pthread.h>
#include <signal.h>

#include "logger.h"
#include "inlist.hpp"

namespace bblocks {

using namespace std;

//..................................................................................... SysConf ....

class SysConf
{
public:

	static uint32_t NumCores()
	{
		uint32_t numCores = sysconf(_SC_NPROCESSORS_ONLN);
		ASSERT(numCores >= 1);

		return numCores;
	}

	static bool SetMaxOpenFds(const size_t size)
	{
		rlimit rl;
		rl.rlim_max = rl.rlim_cur = size + 1;

		int status = setrlimit(RLIMIT_NOFILE, &rl);
		ASSERT(status == 0);

		return status == 0;
	}
};

//..................................................................................... RRCpuId ....

class RRCpuId : public Singleton<RRCpuId>
{
public:

	friend class Singleton<RRCpuId>;

	uint32_t GetId()
	{
		return nextId_++ % SysConf::NumCores();
	}

private:

	RRCpuId()
	{
		nextId_ = 0;
	}

	uint32_t nextId_;
};

}
