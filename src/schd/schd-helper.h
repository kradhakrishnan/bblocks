#ifndef _IOCORE_SCHEDULER_H_
#define _IOCORE_SCHEDULER_H_

#include <inttypes.h>
#include <sys/resource.h>
#include <pthread.h>
#include <boost/shared_ptr.hpp>
#include <signal.h>

#include "logger.h"
#include "inlist.hpp"

namespace dh_core {

class Thread;

//............................................................................... ThreadContext ....

#define SLAB_DEPTH 4

struct ThreadCtx
{
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

	/* Thread instance */
	static __thread Thread * tinst_;

	static void Init(Thread * tinst)
	{
		tinst_ = tinst;
		pool_ = new std::list<uint8_t *>[SLAB_DEPTH];
	}

	static void Cleanup()
	{
		for (int i = 0; i < SLAB_DEPTH; ++i) {
			auto l = ThreadCtx::pool_[i];
			for (auto it = l.begin(); it != l.end(); ++it) {
				::free(*it);
			}

			l.clear();
		}

		delete[] ThreadCtx::pool_;
		ThreadCtx::pool_ = NULL;
	}

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

#endif
