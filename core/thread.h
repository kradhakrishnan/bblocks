#ifndef _DH_CORE_THREAD_H_
#define _DH_CORE_THREAD_H_

#include "core/util.hpp"

namespace dh_core {


class Thread
{
public:

    Thread(const std::string & logPath)
        : log_(logPath)
    {
    }

    void StartThread()
    {
        int ok = pthread_create(&tid_, /*attr=*/ NULL, ThFn, (void *)this);
        ASSERT(!ok);

        SetProcessorAffinity();

        INFO(log_) << "Thread " << tid_ << " created.";
    }

    virtual ~Thread()
    {
        INFO(log_) << "Thread " << tid_ << " detached.";

        int ok = pthread_detach(tid_);
        ASSERT(!ok);
    }

    void SetProcessorAffinity()
    {
        const uint32_t core = RoundRobinCpuId::Instance().GetId();

        INFO(log_) << "Binding to core " << core;

        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        CPU_SET(core, &cpuset);

        int status = pthread_setaffinity_np(tid_, sizeof(cpuset), &cpuset);
        ASSERT(!status);
    }

    static void * ThFn(void * args)
    {
        Thread * th = (Thread *) args;
        return th->ThreadMain();
    }

private:

    virtual void * ThreadMain() = 0;

    LogPath log_;
    pthread_t tid_;
};

} // namespace dh_core

#endif
