#ifndef _IOCORE_SCHEDULER_H_
#define _IOCORE_SCHEDULER_H_

#include <inttypes.h>

#include <pthread.h>
#include <boost/shared_ptr.hpp>
#include <signal.h>

#include "core/logger.h"
#include "core/inlist.hpp"

namespace dh_core {

class Scheduler;

/**
 *
 */
class SysConf
{
public:

    static uint32_t NumCores()
    {
        uint32_t numCores = sysconf(_SC_NPROCESSORS_ONLN);
        ASSERT(numCores >= 1);

        return numCores;
    }
};

/**
 *
 */
class RRCpuId : public Singleton<RRCpuId>
{
public:

    friend class Singleton<RRCpuId>;

    uint32_t GetId()
    {
        return nextId_++ % SysConf::NumCores();
    }

private:

    RRCpuId()
    {
        nextId_ = 0;
    }

    uint32_t nextId_;
};


/**
 *
 */
class Schedulable : public InListElement<Schedulable>
{
public:

    Schedulable(const std::string & name,
                const uint32_t threadAff)
        : name_(name)
        , threadAff_(threadAff)
    {}

    virtual bool Execute() = 0;

    std::string name_;
    uint32_t threadAff_;
};

/**
 *
 */
class SchedulerThread
{
public:

    friend class Scheduler;

    SchedulerThread(const uint32_t & id, const uint32_t & processor_id = 0)
        : log_("/Th/" + STR(id))
        , id_(id)
        , processor_id_(processor_id)
        , schedulables_("scheduler")
        , exitMain_(false)
    {
        int ok = pthread_create(&tid_, /*attr=*/ NULL, ThreadMain, (void *)this);
        ASSERT(!ok);

        SetProcessorAffinity();

        INFO(log_) << "Thread " << id << " created.";
    }

    virtual ~SchedulerThread()
    {
    }

    void EnableThreadCancellation()
    {
        int status = pthread_setcancelstate(PTHREAD_CANCEL_ENABLE,
                                            /*oldstate=*/ NULL);
        ASSERT(!status);
    }

    void DisableThreadCancellation()
    {
        int status = pthread_setcancelstate(PTHREAD_CANCEL_DISABLE,
                                            /*oldstate=*/ NULL);
        ASSERT(!status);
    }

    void Join()
    {
        ASSERT(!exitMain_);
        exitMain_ = true;

        int status = pthread_cancel(tid_);
        ASSERT(!status);

        DEBUG(log_) << "Waiting for MainLoop to exit.";

        status = pthread_join(tid_, /*exit status=*/ NULL);
        ASSERT(!status);
    }

    void SetProcessorAffinity()
    {
        const uint32_t core = processor_id_ % SysConf::NumCores();

        INFO(log_) << "Binding to core " << core;

        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        CPU_SET(core, &cpuset);

        int status = pthread_setaffinity_np(tid_, sizeof(cpuset), &cpuset);
        ASSERT(!status);
    }

    static void * ThreadMain(void * args)
    {
        SchedulerThread * th = (SchedulerThread *) args;
        return th->MainLoop();
    }

    void Schedule(Schedulable * s)
    {
        schedulables_.Push(s);
    }

    void * MainLoop();

private:

    typedef InQueue<Schedulable> schedulables_t;

    LogPath log_;
    const uint32_t id_;
    const uint32_t processor_id_;
    schedulables_t schedulables_;
    pthread_t tid_;
    bool exitMain_;
};

class Scheduler : public Singleton<Scheduler>
{
public:

    friend class Singleton<Scheduler>;

    void InitThreads(uint32_t processors = 1);
    void DestroyThreads();

    uint32_t GetProcessors() const
    {
        return processors_;
    }

    void Schedule(Schedulable * s)
    {
        ASSERT(!threads_.empty());

        DEBUG(log_) << "Scheduling " << s->name_;

        ASSERT(s->threadAff_ < threads_.size());
        SchedulerThread * th = threads_[s->threadAff_];
        th->Schedule(s);
    }

    void Shutdown()
    {
        ShutdownInternal();
    }

    void WaitForStop()
    {
        AutoLock _(waiterLock_.Get());
        exitCondition_.Wait(waiterLock_.Get());
    }

private:

    Scheduler()
        : log_("scheduler/")
        , processors_(UINT32_MAX)
        , waiterLock_(new PThreadMutex(/*isRecursive=*/ false))
    {
        INFO(log_) << "Scheduler created."
                   << " Threads=" << processors_
                   << " Cores=" << SysConf::NumCores();

        srand(time(NULL));
        RRCpuId::Init();
    }

    ~Scheduler()
    {
        ShutdownInternal();
        RRCpuId::Destroy();
    }

    void ShutdownInternal()
    {
        AutoLock _(waiterLock_.Get());
        DestroyThreads();
        exitCondition_.Broadcast();
    }

    typedef std::vector<SchedulerThread *> threads_t;

    LogPath log_;
    threads_t threads_;
    uint32_t processors_;
    AutoPtr<PThreadMutex> waiterLock_;
    WaitCondition exitCondition_;
};

} // namespace dh_core {

#endif
