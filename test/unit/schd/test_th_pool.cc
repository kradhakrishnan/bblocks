#include <iostream>

#include "buf/bufpool.h"
#include "test/unit/unit-test.h"

using namespace dh_core;
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
		ThreadPool::Schedule(this, &This::Dalloc, o);
	}

	void Dalloc(TestObject * o)
	{
		BufferPool::Dalloc(o);

		if (++count_ == 99) {
			ThreadPool::Wakeup();
		}
	}

	std::atomic<int> count_;
};

void
bufferpool_test()
{
    ThreadPool::Start();

    BufferPoolTest test;
    for (int i = 0; i < 100; ++i) {
		ThreadPool::Schedule(&test, &BufferPoolTest::Alloc, i);
    }

    ThreadPool::Wait();
    ThreadPool::Shutdown();
}

//.................................................................................. SimpleTest ....

struct Th
{
    Th() : i_(0) {}

    virtual void Run(Th * th)
    {
        i_++;
        if (i_ > 1000) {
            ThreadPool::Wakeup();
            return;
        } else {
            ThreadPool::Schedule(th, &Th::Run, this);
        }
    }

    std::atomic<uint64_t> i_;

};

void
simple_test()
{
    ThreadPool::Start();

    Th th1;
    Th th2;

    th1.Run(&th2);

    ThreadPool::Wait();
    ThreadPool::Shutdown();
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
        ThreadPool::Schedule(this, &ThMaster::Run, th);
    }

    virtual void Run(ThSlave * th)
    {
	++i_;
	--out_;

        if (i_ > 1000) {
            if (!out_) ThreadPool::Wakeup();
            return;
        }

        ++out_;
        ThreadPool::Schedule(th, &ThSlave::Run, this);
    }

    std::atomic<int> i_;
    std::atomic<int> out_;
};

void ThSlave::Run(ThMaster * th)
{
    ThreadPool::Schedule(th, &ThMaster::Run, this);
}

void
parallel_test()
{
    ThreadPool::Start();

    ThMaster m;
    std::vector<ThSlave *> ss;
    for (unsigned int i = 0; i < SysConf::NumCores() - 1; ++i) {
        ThSlave * s = new ThSlave();
        m.Start(s);
        ss.push_back(s);
    }

    ThreadPool::Wait();
    ThreadPool::Shutdown();

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
