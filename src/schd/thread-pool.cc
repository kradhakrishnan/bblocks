#include "schd/schd-helper.h"
#include "schd/thread-pool.h"

using namespace dh_core;

__thread uint32_t ThreadCtx::tid_;
__thread std::list<uint8_t *> * ThreadCtx::pool_;

static void DestroyBufferPool()
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

void *
NonBlockingThread::ThreadMain()
{
	ThreadCtx::tid_ = tid_;
	ThreadCtx::pool_ = new std::list<uint8_t *>[SLAB_DEPTH];

	DisableThreadCancellation();

	while (!exitMain_)
	{
		ThreadRoutine * r = q_.Pop();
		r->Run();
	}

	DestroyBufferPool();

	INVARIANT(q_.IsEmpty());

	return NULL;
}

bool
NonBlockingThreadPool::ShouldYield()
{
	INVARIANT(ThreadCtx::tid_ != UINT32_MAX);
	INVARIANT(ThreadCtx::tid_ < threads_.size());

	return !threads_[ThreadCtx::tid_]->IsEmpty();
}

