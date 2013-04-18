#include <iostream>

#include "core/thread-pool.h"
#include "core/fnptr.hpp"
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

class Callee
{
public:

    void Callback(int & val)
    {
        cout << "Got callback " << val << endl;
        if (val == 100) {
            NonBlockingThreadPool::Instance().Shutdown();
            return;
        }

        ASSERT(val < 100);
    }
};

class Caller
{
public:

    static const uint64_t TEST = 1024;

    Caller(const boost::shared_ptr<Fn1<int> > & cb) : cb_(cb) {}

    void Start(int val)
    {
        NonBlockingThreadPool::Instance().Schedule(new Fn1ThreadRoutine<int>(cb_, val));
    }

    boost::shared_ptr<Fn1<int> > cb_;
};

void
simple_test()
{
    NonBlockingThreadPool::Instance().Start(/*maxCores=*/ 4);

    Callee callee;
    Caller caller(make_fnptr(&callee, &Callee::Callback));
    for (int i = 0; i <= 100; ++i) {
        NonBlockingThreadPool::Instance().Schedule(&caller, &Caller::Start,
                                                   /*val=*/ (int) i);
    }

    NonBlockingThreadPool::Instance().Wait();
}

int
main(int argc, char ** argv)
{
    InitTestSetup();

    simple_test();
    parallel_test();

    TeardownTestSetup();

    return 0;
}
