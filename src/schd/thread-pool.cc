#include "schd/schd-helper.h"
#include "schd/thread-pool.h"

using namespace std;

using namespace bblocks;

//
// ThreadCtx
//

__thread Thread * ThreadCtx::tinst_;
__thread std::list<uint8_t *> * ThreadCtx::pool_;

LogPath ThreadCtx::log_("/threadctx");

//
// NonBlockingThread
//

void *
NonBlockingThread::ThreadMain()
{
	DisableThreadCancellation();

	try {
		while (true)
		{
			ThreadRoutine * r = q_.Pop();
			r->Run();
		}
	} catch (ThreadExitException & e) {
	    /*
	     * This is ok. This is a little trick we used to exit the thread gracefully
	     */
	}

	INVARIANT(q_.IsEmpty());

	return NULL;
}

bool
NonBlockingThreadPool::ShouldYield()
{
	ASSERT(ThreadCtx::tinst_);

	return ThreadCtx::tinst_->ShouldYield();
}

