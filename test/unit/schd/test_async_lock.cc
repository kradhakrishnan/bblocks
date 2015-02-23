#include "test/unit/unit-test.h"

#include <string>
#include <iostream>

#include "schd/async-lock.hpp"

using namespace bblocks;
using namespace std;

static const string _log = "/test_async_lock";

// ................................................................................. TestBasic ....

class TestBasicCase
{
public:

	typedef TestBasicCase This;

	static const int MAX_TASK = 100;
	static const int MAX_MSG = 5000;

	TestBasicCase(const string path = "/test_async_lock")
		: lock_(path)
		, workerCount_(0)
		, i_(0)
	{
	}

	void Lock(int)
	{
		lock_.Lock(intr_fn(this, &This::Locked));
	}

	void Locked(int)
	{
		AutoUnlock _(&lock_);

		INVARIANT(lock_.IsLocked());

		int j = i_;
		i_ += 122;
		i_ = j;
		i_++;

		if (i_ >= MAX_MSG) {
			workerCount_--;
			if (!workerCount_) {
				BBlocks::Wakeup();
			}

			return;
		}

		BBlocks::Schedule(this, &This::Lock, /*arg=*/ 0);
	}


	static void Run()
	{
		BBlocks::Start();

		TestBasicCase t;
		for (int i = 1; i <= MAX_TASK; i++) {
			t.workerCount_++;
			BBlocks::Schedule(&t, &TestBasicCase::Lock, /*arg=*/ i);
		}

		BBlocks::Wait();
		BBlocks::Shutdown();
	}


	AsyncLock lock_;
	atomic<size_t> workerCount_;
	int i_;
};


//........................................................................................ main ....

int
main(int argc, char ** argv)
{
    InitTestSetup();

    TEST(TestBasicCase::Run);

    TeardownTestSetup();

    return 0;
}

