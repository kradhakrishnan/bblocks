#include <iostream>

#include "test/unit-test.h"

using namespace dh_core;
using namespace std;

class ICallee
{
public:

    virtual void Handle(int val) = 0;

};

class Callee : public ICallee
{
public:

    Callee() : count_(0) {}

    void Handle(int val)
    {
        cout << "Got handle " << count_ << endl;
        ASSERT(val == 0xfeaf);
        if (++count_ == 100) {
            ThreadPool::Shutdown();
        }
    }

    void Callback(int val)
    {
        cout << "Got callback " << count_ << endl;
        ASSERT(val == 0xfeaf);
        if (++count_ == 100) {
            ThreadPool::Shutdown();
        }
    }

    int count_;
};

class Caller
{
public:

    static const uint64_t TEST = 1024;

    Caller(ICallee * h = NULL) : h_(h) {}

    void StartHandler(int val)
    {
        ASSERT(h_);
        ThreadPool::Schedule(h_, &ICallee::Handle, val);
    }

    void StartCallback(int val, Callback<int> * cb)
    {
        ASSERT(cb && !h_);
        cb->ScheduleCallback(val);
    }

    ICallee *h_;
};

void
test_handler()
{
    ThreadPool::Start(/*maxCores=*/ 4);

    Callee callee;
    Caller caller(&callee);
    for (int i = 0; i < 100; ++i) {
        ThreadPool::Schedule(&caller, &Caller::StartHandler,
                             /*val=*/ (int) 0xfeaf);
    }

    ThreadPool::Wait();
}


void
test_callback()
{
    ThreadPool::Start(/*maxCores=*/ 4);

    Callee callee;
    Caller caller;
    for (unsigned int i = 0; i < 100; ++i) {
        ThreadPool::Schedule(&caller, &Caller::StartCallback,
                            /*val=*/ (int) 0xfeaf,
                            make_cb(&callee, &Callee::Callback));
    }

    ThreadPool::Wait();
}

int
main(int argc, char ** argv)
{
    InitTestSetup();

    TEST(test_handler);
    TEST(test_callback);

    TeardownTestSetup();

    return 0;
}
