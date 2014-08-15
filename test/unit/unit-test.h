#pragma once

#include "bblocks.h"
#include "atomic.h"
#include "util.hpp"
#include "schd/thread-pool.h"

#define TEST(x) { \
	cout << "-----------------------------" << endl; \
	cout << #x << endl; x(); \
}

using namespace bblocks;
using namespace std;

//.................................................... time based functions ....

#define SEC2MS(x) (x * 1000)
#define MS2SEC(x) (x / 1000)
#define B2MB(x) (x / (1024 * 1024))

static uint64_t NowInMilliSec()
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

