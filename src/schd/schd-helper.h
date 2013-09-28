#ifndef _IOCORE_SCHEDULER_H_
#define _IOCORE_SCHEDULER_H_

#include <inttypes.h>

#include <pthread.h>
#include <boost/shared_ptr.hpp>
#include <signal.h>

#include "logger.h"
#include "inlist.hpp"

namespace dh_core {

//............................................................................... ThreadContext ....

#define SLAB_DEPTH 4

struct ThreadCtx
{
	static __thread uint32_t tid_;

	/*
	 * Per thread pool
	 *
	 * The slab sizes are
	 * 0 : 0    - 512
	 * 1 : 512  - 1024
	 * 2 : 1024 - 2048
	 * 3 : 2048 - 5196
	 */
	static __thread std::list<uint8_t *> * pool_;
};

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

#endif
