#include <string>
#include <iostream>

#include "test/unit/unit-test.h"

using namespace bblocks;
using namespace std;

static const string _log = "/test_call_later";

// .............................................................................. TestParallel ....

class TestParallel
{
public:

	static const int MAX_MSG = 5000;

	TestParallel() : count_(0) {}

	void Called(int)
	{
		DEBUG(_log) << "Called. count=" << count_;

		count_++;

		if (count_ == 2 * MAX_MSG) {
		    BBlocks::Wakeup();
		}
	}

	static void Run()
	{
		BBlocks::Start();

		TestParallel t;
		for (int i = 1; i <= MAX_MSG; i++) {
			BBlocks::ScheduleIn(/*msec=*/ i, &t, &TestParallel::Called, i);
		}

		for (int i = MAX_MSG; i > 0; i--) {
			BBlocks::ScheduleIn(/*msec=*/ i, &t, &TestParallel::Called, i);
		}

		BBlocks::Wait();
		BBlocks::Shutdown();
	}

	atomic<int> count_;
};

// ................................................................................. TestBasic ....

class TestBasicCase
{
public:

	static const int MAX_MSG = 5000;

	TestBasicCase() : prevmsec_(0) {}

	void Called(uint64_t start, int msec)
	{
		DEBUG(_log) << "Called. msec=" << msec;

		const uint64_t now = Time::NowInMilliSec();

		INVARIANT(now >= (start + msec));
		INVARIANT(prevmsec_ + 1 == msec);

		prevmsec_ = msec;

		if (msec == MAX_MSG) {
		    BBlocks::Wakeup();
		}
	}

	void Reset()
	{
		prevmsec_ = 0;
	}

	static void Run()
	{
		BBlocks::Start(/*ncpu=*/ 1);

		TestBasicCase t;
		for (int i = 1; i <= MAX_MSG; i++) {
			BBlocks::ScheduleIn(/*msec=*/ i, &t, &TestBasicCase::Called,
					    Time::NowInMilliSec(), /*arg=*/ i);
		}

		BBlocks::Wait();

		t.Reset();

		for (int i = MAX_MSG; i > 0; i--) {
			BBlocks::ScheduleIn(/*msec=*/ i, &t, &TestBasicCase::Called,
					    Time::NowInMilliSec(), /*arg=*/ i);
		}

		BBlocks::Wait();
		BBlocks::Shutdown();
	}

	int prevmsec_;
};


//........................................................................................ main ....

int
main(int argc, char ** argv)
{
    InitTestSetup();

    TEST(TestBasicCase::Run);
    TEST(TestParallel::Run);

    TeardownTestSetup();

    return 0;
}
