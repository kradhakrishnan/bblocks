#ifndef _DH_CORE_THREADPOOL_H_
#define _DH_CORE_THREADPOOL_H_

#include "core/thread.h"

namespace dh_core {

class NonBlockingThread;

struct ThreadRoutine : public InListElement<ThreadRoutine>
{
    virtual void Run() = 0;
    virtual ~ThreadRoutine() {}
};

template<class _OBJ_, class _PARAM_>
struct MemberFnPtr : public ThreadRoutine
{

    MemberFnPtr(_OBJ_ * obj, void (_OBJ_::*fn)(_PARAM_), const _PARAM_ & param)
        : obj_(obj), fn_(fn), param_(param)
    {
    }

    virtual void Run()
    {
        (*obj_.*fn_)(param_);
    }

    _OBJ_ * obj_;
    void (_OBJ_::*fn_)(_PARAM_);
    _PARAM_ param_;
}
ALIGNED(sizeof(uint64_t));

template<class _OBJ_, class _PARAM_>
MemberFnPtr<_OBJ_, _PARAM_> *
fn(_OBJ_ * obj, void (_OBJ_::*fn)(_PARAM_), const _PARAM_ & param)
{
    return new MemberFnPtr<_OBJ_, _PARAM_>(obj, fn, param);
}

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

class NonBlockingThread : public Thread
{
public:

    NonBlockingThread(const std::string & path)
        : Thread(path)
        , q_(path)
    {
    }

    virtual void * ThreadMain()
    {
        while (!exitMain_)
        {
            EnableThreadCancellation();
            ThreadRoutine * r = q_.Pop();
            DisableThreadCancellation();

            r->Run();
            delete r;
        }

        return NULL;
    }

    void Push(ThreadRoutine * r)
    {
        q_.Push(r);
    }

private:

    InQueue<ThreadRoutine> q_;
};

class NonBlockingThreadPool : public Singleton<NonBlockingThreadPool>
{
public:

    NonBlockingThreadPool()
        : nextTh_(0)
    {
    }

    void Start(const uint32_t maxCores)
    {
        AutoLock _(&lock_);

        for (size_t i = 0; i < maxCores; ++i) {
            NonBlockingThread * th = new NonBlockingThread("/th/" + STR(i));
            threads_.push_back(th);
            th->StartThread();
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


    template<class _OBJ_, class _PARAM_>
    void Schedule(_OBJ_ * obj, void (_OBJ_::*fn)(_PARAM_),
                  const _PARAM_ & param)
    {
        ThreadRoutine * r = new MemberFnPtr<_OBJ_, _PARAM_>(obj, fn, param);
        threads_[obj->thAffinity_ % threads_.size()]->Push(r);
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

} // namespace dh_core

#endif
