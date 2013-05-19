#ifndef _DH_CORE_THREAD_H_
#define _DH_CORE_THREAD_H_

#include "core/logger.h"
#include "core/util.hpp"
#include "core/scheduler.h"

namespace dh_core {


class Thread
{
public:

    Thread(const std::string & logPath)
        : log_(logPath)
        , exitMain_(false)
    {
    }

    void StartBlockingThread()
    {
        int ok = pthread_create(&tid_, /*attr=*/ NULL, ThFn, (void *)this);
        INVARIANT(!ok);

        // SetProcessorAffinity();

        INFO(log_) << "Thread " << tid_ << " created.";
    }


    void StartNonBlockingThread()
    {
        int ok = pthread_create(&tid_, /*attr=*/ NULL, ThFn, (void *)this);
        INVARIANT(!ok);

        // SetProcessorAffinity();

        INFO(log_) << "Thread " << tid_ << " created.";
    }

    virtual ~Thread()
    {
        INFO(log_) << "Thread " << tid_ << " destroyed.";
    }

    void EnableThreadCancellation()
    {
        int status = pthread_setcancelstate(PTHREAD_CANCEL_ENABLE,
                                            /*oldstate=*/ NULL);
        (void) status;
        ASSERT(!status);
    }

    void DisableThreadCancellation()
    {
        int status = pthread_setcancelstate(PTHREAD_CANCEL_DISABLE,
                                            /*oldstate=*/ NULL);
        (void) status;
        ASSERT(!status);
    }

    void Stop()
    {
        ASSERT(!exitMain_);
        exitMain_ = true;

        int status = pthread_cancel(tid_);
        INVARIANT(!status);

        DEBUG(log_) << "Waiting for MainLoop to exit.";

        status = pthread_join(tid_, /*exit status=*/ NULL);
        // ASSERT(!status);
    }

    void SetProcessorAffinity()
    {
        const uint32_t core = RRCpuId::Instance().GetId();

        INFO(log_) << "Binding to core " << core;

        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        CPU_SET(core, &cpuset);

        int status = pthread_setaffinity_np(tid_, sizeof(cpuset), &cpuset);
        INVARIANT(!status);
    }

    static void * ThFn(void * args)
    {
        Thread * th = (Thread *) args;
        return th->ThreadMain();
    }

protected:

    virtual void * ThreadMain() = 0;

    LogPath log_;
    pthread_t tid_;
    bool exitMain_;
};

} // namespace dh_core

#endif
