#include <iostream>

#include "core/thread-pool.h"
#include "core/atomic.h"

using namespace dh_core;
using namespace std;

void InitTestSetup()
{
    LogHelper::InitConsoleLogger();
    RRCpuId::Init();
    NonBlockingThreadPool::Init();
}

void TeardownTestSetup()
{
    NonBlockingThreadPool::Destroy();
    RRCpuId::Destroy();
    LogHelper::DestroyLogger();
}

class ICallee
{
public:

    virtual void Callback(int val) = 0;

};

class Callee : public ICallee
{
public:

    Callee() : count_(0) {}

    void Callback(int val)
    {
        cout << "Got callback " << count_ << endl;
        ASSERT(val == 0xfeaf);
        if (++count_ == 100) {
            NonBlockingThreadPool::Instance().Shutdown();
        }
    }

    int count_;
};

class Caller
{
public:

    static const uint64_t TEST = 1024;

    Caller(ICallee * cb) : cb_(cb) {}

    void Start(int val)
    {
        NonBlockingThreadPool::Instance().Schedule(cb_, &ICallee::Callback, val);
    }

    ICallee *cb_;
};

void
simple_test()
{
    NonBlockingThreadPool::Instance().Start(/*maxCores=*/ 4);

    Callee callee;
    Caller caller(&callee);
    for (int i = 0; i < 100; ++i) {
        NonBlockingThreadPool::Instance().Schedule(&caller, &Caller::Start,
                                                   /*val=*/ (int) 0xfeaf);
    }

    NonBlockingThreadPool::Instance().Wait();
}

int
main(int argc, char ** argv)
{
    InitTestSetup();

    simple_test();

    TeardownTestSetup();

    return 0;
}
