#include "schd/thread-pool.h"

#include "async.h"
#include "schd/schd-helper.h"
#include "schd/watchdog.hpp"

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
NonBlockingThreadPool::NonBlockingThreadPool()
	: nextTh_(0)
	, timekeeper_("/NBTP/time-keeper")
{
	Watchdog::Init();
}

void
NonBlockingThreadPool::Start(const uint32_t ncpu)
{
	INVARIANT(ncpu <= SysConf::NumCores());

	Guard _(&lock_);

	//
	// Start timer
	//
	bool status = timekeeper_.Init();

	if (!status) {
		ERROR("/NBTP") << "Unable to start timekeeper." << strerror(errno);
		DEADEND
	}

	//
	// Start the threads
	//
	for (size_t i = 0; i < ncpu; ++i) {
		NonBlockingThread * th = new NonBlockingThread("/th/" + STR(i), i);
		threads_.push_back(th);
		th->StartNonBlockingThread();
	}

	Watchdog::Instance().Start(threads_.size());
}

void
NonBlockingThreadPool::Shutdown()
{
	/*
	 * This routine can only be called from the main thread
	 */
	INVARIANT(!ThreadCtx::tinst_);

	Guard _(&lock_);

	/* Shutdown timer service */
	timekeeper_.Shutdown();

	/* Kill async processors */
	DestroyThreads();
}



NonBlockingThreadPool::~NonBlockingThreadPool()
{
	Watchdog::Destroy();
}

void *
NonBlockingThread::ThreadMain()
{
	DisableThreadCancellation();

	try {
		while (true)
		{
			ThreadRoutine * r = q_.Pop();

			const uint64_t & startInMicroSec = Time::NowInMicroSec();

			/* Call watchdog check for this and other threads in the system */
			Watchdog::Instance().Wakeup(startInMicroSec);

			if (!r) {
				/*
				 * Timeout set to wakeup watchdog. Go back to waiting for messages
				 */
				continue;
			}

			/* Start watch */
			Watchdog::Instance().StartWatch(id_, startInMicroSec);

			/* Execute */
			r->Run();

			const uint64_t & endInMicroSec = Time::NowInMicroSec();

			/* Cancel watch */
			Watchdog::Instance().CancelWatch(id_, endInMicroSec);

			/* update stats */
			const uint32_t elapsedInMicroSec = endInMicroSec - startInMicroSec;
			statWatchdogTime_.Update(elapsedInMicroSec);
		}
	} catch (ThreadExitException & e) {
		/*
		 * This is ok. This is a little trick we used to exit the thread gracefully
		 * Cancel the watch
		 */

		Watchdog::Instance().CancelWatch(id_, Time::NowInMicroSec());
	}

	INVARIANT(q_.IsEmpty());

	return NULL;
}

bool
NonBlockingThreadPool::ShouldYield()
{
	ASSERT(ThreadCtx::tinst_);

	return Watchdog::Instance().ShouldYield();
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


