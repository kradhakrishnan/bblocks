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
	 * 2 : 1024 - 2048
	 * 3 : 2048 - 5196
	 */
	static __thread pool_t * pool_;

	/* Thread instance */
	static __thread Thread * tinst_;

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

	static string log_;
};

}
