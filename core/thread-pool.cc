#include "core/thread-pool.h"

using namespace dh_core;

__thread uint32_t _tid = UINT32_MAX;

void *
NonBlockingThread::ThreadMain()
{
    _tid = tid_;

    while (!exitMain_)
    {
        EnableThreadCancellation();
        ThreadRoutine * r = q_.Pop();
        DisableThreadCancellation();

        r->Run();
    }

    return NULL;
}

bool
NonBlockingThreadPool::ShouldYield()
{
    INVARIANT(_tid != UINT32_MAX);
    INVARIANT(_tid < threads_.size());

    return !threads_[_tid]->IsEmpty();
}

