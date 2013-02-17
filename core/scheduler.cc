#include "core/scheduler.h"

using namespace dh_core;

void
Scheduler::InitThreads(const uint32_t processors)
{
    INFO(log_) << "Scheduler initialized. processors=" << processors;

    processors_ = processors;

    ASSERT(threads_.empty());
    for (uint32_t i = 0; i < processors; ++i) {
        const uint32_t core = i % SysConf::NumCores();
        SchedulerThread * th = new SchedulerThread(i, core);
        threads_.push_back(th);
    }
}

void
Scheduler::DestroyThreads()
{
    for (threads_t::iterator it = threads_.begin(); it != threads_.end();
         ++it) {
        SchedulerThread * th = *it;
        // Join with the thread
        th->Join();
        // destroy the thread object
        delete th;
    }

    threads_.clear();
}

void *
SchedulerThread::MainLoop()
{
    while (!exitMain_) {

        EnableThreadCancellation();
        Schedulable * s = schedulables_.Pop();
        DisableThreadCancellation();

        ASSERT(s);
        const bool done = s->Execute();

        if (!done) {
            // we have more messages pending in the queue,
            // need to put this back into rotation
            schedulables_.Push(s);
        }
    }

    INFO(log_) << "Thread " << id_ << " exited.";

    return 0;
}
