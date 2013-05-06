#ifndef _DH_CORE_TEST_UNIT_TEST_H_
#define _DH_CORE_TEST_UNIT_TEST_H_

#include "core/thread-pool.h"
#include "core/atomic.h"
#include "core/callback.hpp"

#define TEST(x) cout << #x << endl; x();

using namespace dh_core;
using namespace std;

//.................................................... time based functions ....

#define SEC2MS(x) (x * 1000)
#define MS2SEC(x) (x / 1000)
#define B2MB(x) (x / (1024 * 1024))

uint64_t
NowInMilliSec()
{
    timeval tv;
    int status = gettimeofday(&tv, /*tz=*/ NULL);
    (void) status;
    ASSERT(!status);
    return tv.tv_sec * 1000 + (tv.tv_usec / 1000);
}

class Timer
{
public:

    Timer()
        : start_ms_(NowInMilliSec())
    {}

    uint64_t Elapsed() const
    {
        return NowInMilliSec() - start_ms_;
    }

    static uint64_t Elapsed(const uint64_t start_ms)
    {
        return NowInMilliSec() - start_ms;
    }

    void Reset()
    {
        start_ms_ = NowInMilliSec();
    }

private:

    uint64_t start_ms_;
};

//............................................... thread pool util funtions ....

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

#endif // _DH_CORE_TEST_UNIT_TEST_H_
