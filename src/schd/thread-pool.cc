#include "schd/schd-helper.h"
#include "schd/thread-pool.h"

using namespace std;

using namespace dh_core;

//
// ThreadCtx
//

__thread uint32_t ThreadCtx::tid_;
__thread Thread * ThreadCtx::tinst_;
__thread std::list<uint8_t *> * ThreadCtx::pool_;

//
// Thread
//

atomic<uint32_t> Thread::nextThId_(0);

//
// NonBlockingThread
//

void *
NonBlockingThread::ThreadMain()
{
	DisableThreadCancellation();

	while (true)
	{
		ThreadRoutine * r = q_.Pop();
		r->Run();
	}

	return NULL;
}

bool
NonBlockingThreadPool::ShouldYield()
{
	ASSERT(ThreadCtx::tinst_);

	return ThreadCtx::tinst_->ShouldYield();
}

