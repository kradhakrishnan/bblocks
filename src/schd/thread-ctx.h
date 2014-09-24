#pragma once

#include <inttypes.h>

#include "logger.h"
#include "schd/thread.h"

namespace bblocks {

using namespace std;

class Thread;

//............................................................................... ThreadContext ....

#define SLAB_DEPTH 4

struct ThreadCtx
{
	typedef list<uint8_t *> pool_t;

	/*
	 * Per thread pool
	 *
	 * The slab sizes are
	 * 0 : 0    - 512
	 * 1 : 512  - 1024
	 * 2 : 1024 - 1536
	 * 3 : 1536 - 2048
	 */
	static __thread pool_t * pool_;

	/* Thread instance */
	static __thread Thread * tinst_;

	static const int GC_TIMEOUT_MS = 1000; // every 1s

	static void Init(Thread * tinst)
	{
		INFO(log_) << "Initializing buffer for " << tinst;

		INVARIANT(!pool_);
		INVARIANT(!tinst_);

		tinst_ = tinst;
		pool_ = new pool_t[SLAB_DEPTH];

		if (tinst_) {
			tinst_->ctx_pool_ = pool_;
		}
	}

	static void Cleanup()
	{
		INFO(log_) << "Cleaning up buffers for " << tinst_;

		static bool printstat = true;

		if (printstat) {
			/*
			 * Little trick to prevent printing of stats for every thread
			 */
			INFO(log_) << "GC stat" << statGC_;
			INFO(log_) << "Hits" << statHits_;
			printstat = false;
		}

		if (tinst_) {
			tinst_->ctx_pool_ = NULL;
			tinst_ = NULL;
		}

		Cleanup(pool_);

		pool_ = NULL;
	}

	static void Cleanup(pool_t * pool)
	{
		for (int i = 0; i < SLAB_DEPTH; ++i) {
			auto l = pool[i];
			for (auto it = l.begin(); it != l.end(); ++it) {
				::free(*it);
			}

			l.clear();
		}

		delete[] pool;
	}

	static void GarbageCollect()
	{
		INVARIANT(ThreadCtx::tinst_);
		INVARIANT(ThreadCtx::pool_);

		static __thread uint64_t lastInMilliSec = Rdtsc::NowInMilliSec();

		uint64_t nowInMilliSec = Rdtsc::NowInMilliSec();

		if (Rdtsc::Elapsed(nowInMilliSec, lastInMilliSec) > GC_TIMEOUT_MS) {
			DEBUG(log_) << "GC kicked for " << tinst_;

			/*
			 * Timeout. Delete all unused memory
			 */
			size_t bytes = 0;
			for (int i = 0; i < SLAB_DEPTH; ++i) {
				pool_t & pool = ThreadCtx::pool_[i];

				for (auto ptr : pool) {
					::free((void *) ptr);
					bytes += (i + 1) * 512; 
				}

				pool.clear();
			}

			statGC_.Update(bytes);

			lastInMilliSec = nowInMilliSec;

			DEBUG(log_) << bytes << " B reclaimed by GC on thread " << tinst_;
		}
	}

	static string log_;

	static PerfCounter statGC_;
	static PerfCounter statHits_;
};

}
