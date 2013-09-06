#ifndef _K_IOCORE_ATOMIC_H_
#define _K_IOCORE_ATOMIC_H_

#include <atomic>
#include <inttypes.h>

#include "assert.h"

namespace dh_core {

//............................................................................... AtomicCounter ....

class AtomicCounter
{
public:

	AtomicCounter(size_t val = 0) : count_(val) {}

	uint64_t Add(const int val)
	{
		return count_.fetch_add(val);
	}

	uint64_t Count() const
	{
		return count_.load();
	}

	uint64_t Set(const uint64_t val)
	{
		return count_.exchange(val);
	}

	bool CompareAndSwap(const uint64_t val, uint64_t prev)
	{
		return count_.compare_exchange_strong(prev, val);
	}

private:

	std::atomic<size_t> count_;

};

};

#endif
