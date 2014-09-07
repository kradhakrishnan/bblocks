#pragma once

#include <list>

#include "async.h"
#include "lock.h"

namespace bblocks {

using namespace std;

/**
 * Synchronous locks were originally designed for single core system. Later they found decent
 * adoption with thread pools. But in the context of concurrent systems, they are very wasteful
 * because no body else can be scheduled on the thread unless the current callback yields.
 *
 * One solution to the problem is to synchronize non thread safe operations using synchronized_fn,
 * but that not work if there are different blocks of code that need to be protected by locks.
 * Another solution is to use asynchronous locks.
 */
class AsyncLock : public Mutex
{
public:

	AsyncLock(const string path)
            : path_(path)
            , isLocked_(false)
            , lock_(path_)
        {}

	virtual ~AsyncLock()
	{
                Guard _(&lock_);
                INVARIANT(!isLocked_);
		INVARIANT(waiters_.empty());
	}

	virtual void Lock() override
	{
		DEADEND
	}

	void Lock(Fn<int> fn)
	{
		{
			Guard _(&lock_);

			if (isLocked_) {
				waiters_.push_back(fn);
				return;
			}

			ASSERT(!isLocked_);

			isLocked_ = true;
		}

		fn.Wakeup(/*status=*/ 0);
	}

	virtual void Unlock() override
	{
		Guard autolock(&lock_);

		if (!waiters_.empty()) {
			Fn<int> fn = waiters_.front();
			waiters_.pop_front();
			autolock.Unlock();
			fn.Wakeup(/*status=*/ 0);
			return;
		}

		ASSERT(isLocked_);
		isLocked_ = false;
	}

	virtual bool IsOwner() override
	{
                DEADEND
	}

	bool IsLocked() const
	{
		return isLocked_;
	}

private:

	typedef list<Fn<int> > fns_t;

        const string path_;
	fns_t waiters_;
	atomic<bool> isLocked_;
	SpinMutex lock_;
};

}
