#ifndef _DH_CORE_THREADPOOL_H_
#define _DH_CORE_THREADPOOL_H_

#include "defs.h"
#include "thread.h"

namespace dh_core {

class NonBlockingThread;

// .......................................................... ThreadRoutine ....

/**
 *
 */
class ThreadRoutine : public InListElement<ThreadRoutine>
{
public:

    virtual void Run() = 0;
    virtual ~ThreadRoutine() {}
};

// ........................................................ MemberFnPtr*<*> ....

/**
 *
 */
#define MEMBERFNPTR(n)                                                      \
template<class _OBJ_, TDEF(T,n)>                                            \
class MemberFnPtr##n : public ThreadRoutine                                 \
{                                                                           \
public:                                                                     \
                                                                            \
    MemberFnPtr##n(_OBJ_ * obj, void (_OBJ_::*fn)(TENUM(T,n)),              \
                  TPARAM(T,t,n))                                            \
        : obj_(obj), fn_(fn), TASSIGN(t,n)                                  \
    {                                                                       \
    }                                                                       \
                                                                            \
    virtual void Run()                                                      \
    {                                                                       \
        (obj_->*fn_)(TARGEX(t,_,n));                                        \
        delete this;                                                        \
    }                                                                       \
                                                                            \
private:                                                                    \
                                                                            \
    _OBJ_ * obj_;                                                           \
    void (_OBJ_::*fn_)(TENUM(T,n));                                         \
    TMEMBERDEF(T,t,n);                                                      \
};                                                                          \

MEMBERFNPTR(1)  // MemberFnPtr1<_OBJ_, T1>
MEMBERFNPTR(2)  // MemberFnPtr2<_OBJ_, T1, T2>
MEMBERFNPTR(3)  // MemberFnPtr3<_OBJ_, T1, T2, T3>
MEMBERFNPTR(4)  // MemberFnPtr4<_OBJ_, T1, T2, T3, T4>

// ....................................................... NonBlockingLogic ....

/**
 * TODO: Throw this legacy stuff out pls
 */
class NonBlockingLogic
{
public:

    friend class NonBlockingThreadPool;

    NonBlockingLogic()
        : thAffinity_(RRCpuId::Instance().GetId())
    {
    }

    virtual ~NonBlockingLogic()
    {
    }

protected:

    const uint32_t thAffinity_;
};

// ...................................................... NonBlockingThread ....

/**
 *
 */
class NonBlockingThread : public Thread
{
public:

    NonBlockingThread(const std::string & path, const uint32_t tid)
        : Thread(path)
        , q_(path)
        , tid_(tid)
    {
    }

    virtual void * ThreadMain();

    void Push(ThreadRoutine * r)
    {
        q_.Push(r);
    }

    bool IsEmpty() const
    {
        return q_.IsEmpty();
    }

private:

    InQueue<ThreadRoutine> q_;
    const uint32_t tid_;
};

// .................................................. NonBlockingThreadPool ....

/**
 *
 */
class NonBlockingThreadPool : public Singleton<NonBlockingThreadPool>
{
public:

    friend class NonBlockingThread;

    class BarrierRoutine
    {
    public:

        BarrierRoutine(ThreadRoutine * cb, const size_t count)
            : cb_(cb), pendingCalls_(count)
        {
        }

        void Run(int)
        { 
            const uint64_t count = pendingCalls_.Add(/*count=*/ -1);

            INFO(LogPath("/barrierRoutine")) << "Count=" << count;

            if (count == 1) {
                INVARIANT(!pendingCalls_.Count());
                NonBlockingThreadPool::Instance().Schedule(cb_);
                cb_ = NULL;
                delete this;
            }
        }

    private:

        ThreadRoutine * cb_;
        AtomicCounter pendingCalls_;
    };


    NonBlockingThreadPool()
        : nextTh_(0)
    {
    }

    void Start(const uint32_t maxCores)
    {
        AutoLock _(&lock_);

        for (size_t i = 0; i < maxCores; ++i) {
            NonBlockingThread * th = new NonBlockingThread("/th/" + STR(i), i);
            threads_.push_back(th);
            th->StartNonBlockingThread();
        }
    }

    void Shutdown()
    {
        AutoLock _(&lock_);
        DestroyThreads();
        condExit_.Broadcast();
    }

    void Wait()
    {
        AutoLock _(&lock_);
        condExit_.Wait(&lock_);
    }

    #define NBTP_SCHEDULE(n)                                                \
    template<class _OBJ_, TDEF(T,n)>                                        \
    void Schedule(_OBJ_ * obj, void (_OBJ_::*fn)(TENUM(T,n)),               \
                  TPARAM(T,t,n))                                            \
    {                                                                       \
        ThreadRoutine * r;                                                  \
        r = new MemberFnPtr##n<_OBJ_, TENUM(T,n)>(obj, fn, TARG(t,n));      \
        threads_[nextTh_++ % threads_.size()]->Push(r);                     \
    }                                                                       \

    NBTP_SCHEDULE(1) // void Schedule<T1>(...)
    NBTP_SCHEDULE(2) // void Schedule<T1,T2>(...)
    NBTP_SCHEDULE(3) // void Schedule<T1,T2,T3>(...)
    NBTP_SCHEDULE(4) // void Schedule<T1,T2,T3,T4>(...)

    void Schedule(ThreadRoutine * r)
    {
        threads_[nextTh_++ % threads_.size()]->Push(r);
    }

    bool ShouldYield();

    void ScheduleBarrier(ThreadRoutine * r)
    {
        BarrierRoutine * br = new BarrierRoutine(r, threads_.size());
        for (size_t i = 0; i < threads_.size(); ++i) {
            ThreadRoutine * r = new MemberFnPtr1<BarrierRoutine, int>
                                      (br, &BarrierRoutine::Run, /*status=*/ 0);
            threads_[i]->Push(r);
        }
    }

private:

    typedef std::vector<NonBlockingThread *> threads_t;

    void DestroyThreads()
    {
        for (threads_t::iterator it = threads_.begin(); it != threads_.end();
             ++it) {
            NonBlockingThread * th = *it;
            // Stop the thread
            th->Stop();
            // destroy the thread object
            delete th;
        }

        threads_.clear();
    }


    PThreadMutex lock_;
    threads_t threads_;
    WaitCondition condExit_;
    uint32_t nextTh_;
};

// ............................................................. ThreadPool ....

class ThreadPool
{
public:

    static void Start(const uint32_t ncores)
    {
        NonBlockingThreadPool::Instance().Start(ncores);
    }

    static void Shutdown()
    {
        NonBlockingThreadPool::Instance().Shutdown();
    }

    static void Wait()
    {
        NonBlockingThreadPool::Instance().Wait();
    }

    #define TP_SCHEDULE(n)                                                  \
    template<class _OBJ_, TDEF(T,n)>                                        \
    static void Schedule(_OBJ_ * obj, void (_OBJ_::*fn)(TENUM(T,n)),        \
                         TPARAM(T,t,n))                                     \
    {                                                                       \
        NonBlockingThreadPool::Instance().Schedule(obj, fn, TARG(t,n));     \
    }                                                                       \

    TP_SCHEDULE(1) // void Schedule<T1>(...)
    TP_SCHEDULE(2) // void Schedule<T1,T2>(...)
    TP_SCHEDULE(3) // void Schedule<T1,T2,T3>(...)
    TP_SCHEDULE(4) // void Schedule<T1,T2,T3,T4>(...)

    static void Schedule(ThreadRoutine * r)
    {
        NonBlockingThreadPool::Instance().Schedule(r);
    }

    static void ScheduleBarrier(ThreadRoutine * r)
    {
        NonBlockingThreadPool::Instance().ScheduleBarrier(r);
    }

    static bool ShouldYield()
    {
        return NonBlockingThreadPool::Instance().ShouldYield();
    }
};


} // namespace dh_core

#endif
