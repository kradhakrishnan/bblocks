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

	BufferPoolTest() : count_(0) {}

	void Alloc(int count)
	{
		TestObject * o = new (BufferPool::Alloc<TestObject>()) TestObject();
		BBlocks::Schedule(this, &This::Dalloc, o);
	}

	void Dalloc(TestObject * o)
	{
		BufferPool::Dalloc(o);

		if (++count_ == 99) {
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
    for (int i = 0; i < 100; ++i) {
		BBlocks::Schedule(&test, &BufferPoolTest::Alloc, i);
    }

    BBlocks::Wait();
    BBlocks::Shutdown();
}

//.................................................................................. SimpleTest ....

struct Th
{
    Th() : i_(0) {}

    virtual void Run(Th * th)
    {
        i_++;
        if (i_ > 1000) {
            BBlocks::Wakeup();
            return;
        } else {
            BBlocks::Schedule(th, &Th::Run, this);
        }
    }

    atomic<uint64_t> i_;

};

void
simple_test()
{
    BBlocks::Start();

    Th th1;
    Th th2;

    th1.Run(&th2);

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

    void Start(ThSlave * th)
    {
        ++out_;
        BBlocks::Schedule(this, &ThMaster::Run, th);
    }

    virtual void Run(ThSlave * th)
    {
	++i_;
	--out_;

        if (i_ > 1000) {
            if (!out_) BBlocks::Wakeup();
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

    ThMaster m;
    vector<ThSlave *> ss;
    for (unsigned int i = 0; i < SysConf::NumCores() - 1; ++i) {
        ThSlave * s = new ThSlave();
        m.Start(s);
        ss.push_back(s);
    }

    BBlocks::Wait();
    BBlocks::Shutdown();

    for (unsigned int i = 0; i < ss.size(); ++i) {
        delete ss[i];
    }

    ss.clear();
}

int
main(int argc, char ** argv)
{
    InitTestSetup();

    TEST(bufferpool_test);
    TEST(simple_test);
    TEST(parallel_test);

    TeardownTestSetup();

    return 0;
}
