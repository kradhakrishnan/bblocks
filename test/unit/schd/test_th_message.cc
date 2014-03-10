#include <iostream>

#include "test/unit/unit-test.h"

using namespace dh_core;
using namespace std;

//................................................................................ test_handler ....

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
            ThreadPool::Wakeup();
        }
    }

    int count_;
};

class Caller
{
public:

    static const uint64_t TEST = 1024;

    Caller(ICallee * h = NULL) : h_(h) {}

    void Start(int val)
    {
        ASSERT(h_);
        ThreadPool::Schedule(h_, &ICallee::Handle, val);
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
        ThreadPool::Schedule(&caller, &Caller::Start, /*val=*/ (int) 0xfeaf);
    }

    ThreadPool::Wait();
    ThreadPool::Shutdown();
}

//........................................................................................ main ....

int
main(int argc, char ** argv)
{
    InitTestSetup();

    TEST(test_handler);

    TeardownTestSetup();

    return 0;
}
