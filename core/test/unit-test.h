#ifndef _DH_CORE_TEST_UNIT_TEST_H_
#define _DH_CORE_TEST_UNIT_TEST_H_

#include "core/thread-pool.h"
#include "core/atomic.h"
#include "core/callback.hpp"

#define TEST(x) cout << #x << endl; x();

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

#endif // _DH_CORE_TEST_UNIT_TEST_H_
