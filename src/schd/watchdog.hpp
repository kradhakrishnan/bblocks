#pragma once

#include <set>

#include "async.h"
#include "util.h"
#include "bblocks.h"

namespace bblocks {

using namespace std;

class Watchdog : public Singleton<Watchdog>
{
public:

	using This = Watchdog;

	static const size_t TIMEOUT_MS = 500000; // 500 MilliSec

	Watchdog()
		: path_("/watchdog")
		, ncpu_(0)
		, timestamps_(NULL)
		, stoplock_(path_ + "/stoplock")
		, lastWakeupInMicroSec_(Rdtsc::NowInMicroSec())
	{
	}

	void Start(const size_t ncpu)
	{
		Guard __(&stoplock_);

		ASSERT(!stopFn_);

		ncpu_ = ncpu;

		if (timestamps_) {
			/*
			 * The async processor is restarted. We need to clear up the old timstamp
			 * tracker
			 */
			delete[] timestamps_;
			timestamps_ = NULL;
		}

		timestamps_ = new atomic<uint64_t>[ncpu_];

		for (size_t i = 0; i < ncpu_; i++) {
			timestamps_[i] = UINT64_MAX;
		}

		INFO(path_) << "Watchdog started for " << ncpu_ << " cpu";
	}

	~Watchdog()
	{
		if (timestamps_) {
			delete[] timestamps_;
			timestamps_ = NULL;
		}
	}

	void Wakeup(const uint64_t nowInMicroSec)
	{
		if (!Timeout(nowInMicroSec)) {
			/*
			 * Watchdog timeout not reached
			 */
			return;
		}

		DEBUG(path_) << "Watch dog audit started. time = " << nowInMicroSec;

		lastWakeupInMicroSec_ = nowInMicroSec;

		for (size_t i = 0; i < ncpu_; i++) {
			const atomic<uint64_t> & t = timestamps_[i];
			if (t < nowInMicroSec) {
				/*
				 * There is a record whose timeout already occoured
				 */
				ERROR(path_) << "Watch dog triggered. thread = " << i;
				DEADEND
			}
		}
	}

	void StartWatch(const size_t idx, const uint64_t nowInMicroSec)
	{
		ASSERT(idx <= ncpu_);

		timestamps_[idx] = nowInMicroSec + (TIMEOUT_MS * 1000);

		DEBUG(path_) << "Start watch. thread = " << idx
			     << " time = " << nowInMicroSec;
	}

	void CancelWatch(const size_t idx, const uint64_t nowInMicroSec)
	{
		ASSERT(idx <= ncpu_);

		if (timestamps_[idx] < nowInMicroSec) {
			ERROR(path_) << "Watchdog triggered. thread = " << idx;
			DEADEND
		}

		timestamps_[idx] = UINT64_MAX;

		DEBUG(path_) << "Cancel watch. thread = " << idx;
	}

	bool ShouldYield()
	{
		/*
		 * Watch dog can give a heads up on if it is time to yield. You will have about a
		 * 100ms to reschedule a async call and get out of the path.
		 */

		ASSERT(ThreadCtx::tinst_);

		const uint32_t & id = ((NonBlockingThread *) ThreadCtx::tinst_)->id_;

		ASSERT(id < ncpu_);
		ASSERT(timestamps_[id] != UINT64_MAX);

		const uint32_t elapsedInMicroSec = Rdtsc::ElapsedInMicroSec(timestamps_[id]);

		return elapsedInMicroSec > YIELD_MS;
	}

private:

	static const size_t YIELD_MS = TIMEOUT_MS - 100; // 400ms

	static_assert(TIMEOUT_MS >= (YIELD_MS + 100), "Too high YIELD_MS");

	bool Timeout(const uint64_t nowInMicroSec) const
	{
		return (lastWakeupInMicroSec_ + (TIMEOUT_MS * 1000)) > nowInMicroSec;
	}

	const string path_;
	size_t ncpu_;
	atomic<uint64_t> * timestamps_;
	SpinMutex stoplock_;
	atomic<uint64_t> lastWakeupInMicroSec_;
	Fn<int> stopFn_;
};

}
