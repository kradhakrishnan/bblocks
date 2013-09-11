#include <iostream>

#include "test/unit-test.h"

using namespace dh_core;
using namespace std;

//.................................................................................. SimpleTest ....

struct Th
{
    Th() : i_(0) {}

    virtual void Run(Th * th)
    {
        if (!(i_ % 100000)) {
            cout << this << " : " << i_ << endl;
        }

        i_++;
        if (i_ > 1000000) {
            ThreadPool::Shutdown();
            return;
        } else {
            ThreadPool::Schedule(th, &Th::Run, this);
        }
    }

    uint64_t i_;

};

void
simple_test()
{
    ThreadPool::Start(/*maxCores=*/   1);

    Th th1;
    Th th2;

    th1.Run(&th2);

    ThreadPool::Wait();
}

//................................................................................ ParallelTest ....

class ThMaster;

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
        out_.Add(1);
        ThreadPool::Schedule(this, &ThMaster::Run, th);
    }

    virtual void Run(ThSlave * th)
    {
        if (!(i_.Count() % 1000)) {
            cout << (unsigned int) i_.Count() << " : " << out_.Count() << endl;
        }

        i_.Add(1);
        out_.Add(-1);

        if (i_.Count() > 50000) {
            if (!out_.Count()) {
                ThreadPool::Shutdown();
            }
            return;
        }

        out_.Add(1);
        ThreadPool::Schedule(th, &ThSlave::Run, this);
    }

    AtomicCounter i_;
    AtomicCounter out_;
};

void ThSlave::Run(ThMaster * th)
{
    ThreadPool::Schedule(th, &ThMaster::Run, this);
}

void
parallel_test()
{
    const unsigned int cores = 4;
    ThreadPool::Start(cores);

    ThMaster m;
    std::vector<ThSlave *> ss;
    for (unsigned int i = 0; i < cores - 1; ++i) {
        ThSlave * s = new ThSlave();
        m.Start(s);
        ss.push_back(s);
    }

    ThreadPool::Wait();

    for (unsigned int i = 0; i < ss.size(); ++i) {
        delete ss[i];
    }

    ss.clear();
}

int
main(int argc, char ** argv)
{
    InitTestSetup();

    TEST(simple_test);
    TEST(parallel_test);

    TeardownTestSetup();

    return 0;
}
