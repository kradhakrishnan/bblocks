#include "schd/schd-helper.h"
#include "schd/thread-pool.h"

using namespace std;

using namespace bblocks;

//
// ThreadCtx
//

__thread Thread * ThreadCtx::tinst_;
__thread list<uint8_t *> * ThreadCtx::pool_;

string ThreadCtx::log_("/threadctx");

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

//
// TimeKeeper
//
void *
TimeKeeper::ThreadMain()
{
	INVARIANT(fd_);

	while (true) {
		uint64_t count;
		int status = read(fd_, &count, sizeof(count));

		if (status != sizeof(count)) {
			ERROR(path_) << "Error proceesing timer events." << strerror(errno);
			DEADEND
		}

		Guard _(&lock_);

		INVARIANT(timers_.size() >= count);
		INVARIANT(count == 1);

		/*
		 * Timers are ordered by the wait times, so we can start kicking off
		 * starting from the front
		 */
		const TimerEvent & t = *timers_.begin();
		DEBUG(path_) << "Dispatching for time "
			     << t.time_.tv_sec << "." << t.time_.tv_nsec;

		NonBlockingThreadPool::Instance().Schedule(t.r_);

		timers_.erase(timers_.begin());

		if (!timers_.empty()) {
			INVARIANT(SetTimer());
		}
	}

	DEADEND
}


