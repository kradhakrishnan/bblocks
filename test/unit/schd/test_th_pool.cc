#include <iostream>

#include "bblocks.h"
#include "buf/bufpool.h"
#include "test/unit/unit-test.h"

using namespace bblocks;
using namespace std;

//.............................................................................. BufferPoolTest ....

class TestObject
{
public:

    TestObject() {}
};

struct BufferPoolTest
{
	typedef BufferPoolTest This;

	static const int MAX_CALLS = 100;

	BufferPoolTest() : count_(0) {}

	void Alloc(int count)
	{
		TestObject * o = new (BufferPool::Alloc<TestObject>()) TestObject();
		BBlocks::Schedule(this, &This::Dalloc, o);
	}

	void Dalloc(TestObject * o)
	{
		BufferPool::Dalloc(o);

		if (++count_ == MAX_CALLS) {
			BBlocks::Wakeup();
		}
	}

	atomic<int> count_;
};

void
bufferpool_test()
{
    BBlocks::Start();

    BufferPoolTest test;
    for (int i = 0; i < BufferPoolTest::MAX_CALLS; ++i) {
		BBlocks::Schedule(&test, &BufferPoolTest::Alloc, i);
    }

    BBlocks::Wait();
    BBlocks::Shutdown();
}

//.................................................................................. SimpleTest ....

struct PingPong
{
    PingPong() : i_(0) {}

	static const size_t MAX_CALLS = 1000;

    virtual void Run(PingPong * th)
    {
        i_++;
        if (i_ > MAX_CALLS) {
            BBlocks::Wakeup();
            return;
        } else {
            BBlocks::Schedule(th, &PingPong::Run, this);
        }
    }

    atomic<uint64_t> i_;

};

void
pingpong_test()
{
    BBlocks::Start();

    PingPong ping;
    PingPong pong;

    ping.Run(&pong);

    BBlocks::Wait();
    BBlocks::Shutdown();
}

//................................................................................ ParallelTest ....

struct ThMaster;

struct ThSlave
{
	ThSlave()
	{
		cout << "Slave created." << endl;
	}

	void Run(ThMaster * th);
};

struct ThMaster
{
	ThMaster() : i_(0), out_(0) {}

	void ScheduleSlave(ThSlave * th)
	{
		++out_;
		BBlocks::Schedule(this, &ThMaster::Run, th);
	}

	virtual void Run(ThSlave * th)
	{
		++i_;
		--out_;

		if (i_ > 1000) {
			if (!out_) {
				BBlocks::Wakeup();
			}
            return;
		}

		++out_;
		BBlocks::Schedule(th, &ThSlave::Run, this);
	}

    atomic<int> i_;
    atomic<int> out_;
};

void ThSlave::Run(ThMaster * th)
{
    BBlocks::Schedule(th, &ThMaster::Run, this);
}

void
parallel_test()
{
    BBlocks::Start();

    ThMaster master;
    vector<ThSlave *> slaves;
    for (size_t i = 0; i < SysConf::NumCores(); ++i) {
        ThSlave * s = new ThSlave();
        master.ScheduleSlave(s);
        slaves.push_back(s);
    }

    BBlocks::Wait();
    BBlocks::Shutdown();

    for (size_t i = 0; i < slaves.size(); ++i) {
        delete slaves[i];
    }

    slaves.clear();
}

int
main(int argc, char ** argv)
{
    InitTestSetup();

    TEST(bufferpool_test);
    TEST(pingpong_test);
    TEST(parallel_test);

    TeardownTestSetup();

    return 0;
}
