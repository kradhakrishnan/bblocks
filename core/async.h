#ifndef _DH_CORE_ASYNC_H_
#define _DH_CORE_ASYNC_H_

#include "core/defs.h"
#include "core/buffer.h"
#include "core/thread-pool.h"

namespace dh_core {

/*!
Our async design is an adaptation of Proactor design pattern. Most of the design
ideas are based on Pattern-Oriented Software Architecture, Volume 2, Schmidt et
al., Jon Wiley & Sons, and personal development experience. 

The fundamental goal is to design an asynchronous communication framework that
can be used to tackle asynchronous IO with higher level language like c++. It is
well documented that Proactor model is highly suitable for multicore systems and
can scale much better than other models like reactor model or pure actor model.

A typical proactor model has the following components :

1. Proactor
2. Asynchronous completion handle
3. Asynchronous completion token aka ACT
4. Completion event
5. Completion event demultiplexer
6. Completion event queue 
*/

// helper meta information
// this can help to provide information about functions that language doesn't
// have in itself 

#define __completion_handler__ /* async notification */
#define __interrupt__ /* synchronous callback */
#define __async_operation__ /* async operation function */

//....................................................... CompletionHandler ....

class CompletionHandle
{
public:

    virtual ~CompletionHandle() {}
};

using CHandle = CompletionHandle;

//...................................................... CompletionToken<T> ....

class AsyncCompletionToken
{
};

//.......................................................... AsyncProcessor ....

class AsyncProcessor
{
public:

    virtual ~AsyncProcessor() {}

    typedef void (CHandle::*UnregisterDoneFn)(int);

    /*!
     * \brief Register a completion handle with the async processor
     *
     * \param   h   Completion handle
     */
     virtual void RegisterHandle(CompletionHandle * h) = 0;

    /*!
     * \brief Unregister a previously registered handle
     *
     * Procondition : The handle should have already been registered
     *
     * Typically this function should callback UnregisteredHandle after draining
     * all the scheduled events to the client and after relinquishing all
     * resources shared with this specified client.
     *
     * \param   h   Completion handle reference
     * \param   cb  Callback after the handle is safely unregistered
     */
     __async_operation__
     virtual void UnregisterHandle(CompletionHandle * h,
                                   const UnregisterDoneFn cb) = 0;
};

#define COMPLETION_QUEUE(TSUFFIX, n)                                            \
template<TDEF(T,n)>                                                             \
class CompletionQueue##TSUFFIX : public CHandle                                 \
{                                                                               \
public:                                                                         \
                                                                                \
    typedef CompletionQueue##TSUFFIX<TENUM(T,n)> This;                          \
                                                                                \
    CompletionQueue##TSUFFIX(CHandle * h, void (CHandle::*fn)(TENUM(T,n)))      \
        : h_(h), fn_(fn)                                                        \
    {}                                                                          \
                                                                                \
    ~CompletionQueue##TSUFFIX()                                                 \
    {                                                                           \
        INVARIANT(q_.empty());                                                  \
        INVARIANT(!processorRunning_);                                          \
    }                                                                           \
                                                                                \
    void Wakeup(TPARAM(T,t,n))                                                  \
    {                                                                           \
        Guard _(&lock_);                                                        \
                                                                                \
        q_.push_back(CompletionEvent(TARG(t,n)));                               \
                                                                                \
        if (!processorRunning_) {                                               \
            ASSERT(q_.size() == 1);                                             \
            processorRunning_ = true;                                           \
            ThreadPool::Schedule(this, &This::ProcessEvents, /*nonce=*/ 0);     \
        }                                                                       \
    }                                                                           \
                                                                                \
private:                                                                        \
                                                                                \
    void ProcessEvents(int)                                                     \
    {                                                                           \
        std::list<CompletionEvent> q;                                           \
                                                                                \
        /* we are like epoll, we take all the events that have matured and      \
           process them                                                         \
         */                                                                     \
                                                                                \
        {                                                                       \
            Guard _(&lock_);                                                    \
            processorRunning_ = true;                                           \
            q = q_;                                                             \
            q_.clear();                                                         \
                                                                                \
            if (q.empty()) {                                                    \
                processorRunning_ = false;                                      \
                return;                                                         \
            }                                                                   \
        }                                                                       \
                                                                                \
        ASSERT(!q_.empty());                                                    \
                                                                                \
        for (auto it = q.begin(); it != q.end(); ++it) {                        \
            CompletionEvent & e = *it;                                          \
            (h_->*fn_)(TARGEX(e.t,_,n));                                    \
        }                                                                       \
                                                                                \
        ThreadPool::Schedule(this, &This::ProcessEvents, /*nonce=*/ 0);         \
    }                                                                           \
                                                                                \
    struct CompletionEvent                                                      \
    {                                                                           \
        CompletionEvent(TPARAM(T,t,n))                                          \
            : TASSIGN(t,n)                                                      \
        {}                                                                      \
                                                                                \
        TMEMBERDEF(T,t,n)                                                       \
    };                                                                          \
                                                                                \
    SpinMutex lock_;                                                            \
    std::list<CompletionEvent> q_;                                              \
    CHandle * h_;                                                               \
    void (CHandle::*fn_)(TENUM(T,n));                                           \
    bool processorRunning_;                                                     \
};                                                                              \

COMPLETION_QUEUE( , 1) // CompletionQueue<T>
COMPLETION_QUEUE(2, 2) // CompletionQueue2<T1,T2>
COMPLETION_QUEUE(3, 3) // CompletionQueue3<T1,T2,T3>

COMPLETION_QUEUE(WithCtx , 2) // CompletionQueueWithCtx<T,TCTX>
COMPLETION_QUEUE(WithCtx2, 3) // CompletionQueue2<T1,T2,TCTX>
COMPLETION_QUEUE(WithCtx3, 4) // CompletionQueue3<T1,T2,T3,TCTX>

#define COMPLETION_HANDLER(TSUFFIX, n)                                          \
template<TDEF(T,n)>                                                             \
class CompletionHandler##TSUFFIX                                                \
{                                                                               \
public:                                                                         \
                                                                                \
    typedef enum                                                                \
    {                                                                           \
        INTERRUPT = 0,                                                          \
        ASYNCSCHEDULE,                                                          \
        QUEUE,                                                                  \
    }                                                                           \
    cbtype_t;                                                                   \
                                                                                \
    CompletionHandler##TSUFFIX(cbtype_t type, CHandle * h,                      \
                               void (CHandle::*fn)(TPARAM(T,t,n)))              \
        : type_(type)                                                           \
        , h_(h)                                                                 \
        , fn_(fn)                                                               \
        , fnWithCtx_(NULL)                                                      \
        , q_(NULL)                                                              \
        , qWithCtx_(NULL)                                                       \
        , ctx_(0)                                                               \
    {}                                                                          \
                                                                                \
    CompletionHandler##TSUFFIX(cbtype_t type, CHandle * h,                      \
                               void (CHandle::*fnWithCtx)(TPARAM(T,t,n),        \
                                     uintptr_t),                                \
                               uintptr_t ctx)                                   \
        : type_(type)                                                           \
        , h_(h)                                                                 \
        , fn_(NULL)                                                             \
        , fnWithCtx_(fnWithCtx)                                                 \
        , q_(NULL)                                                              \
        , qWithCtx_(NULL)                                                       \
        , ctx_(ctx)                                                             \
    {}                                                                          \
                                                                                \
    void Interrupt(TPARAM(T,t,n))                                               \
    {                                                                           \
        INVARIANT(type_ == INTERRUPT);                                          \
        (h_->*fn_)(TARG(t,n));                                                  \
    }                                                                           \
                                                                                \
    void Wakeup(TPARAM(T,t,n))                                                  \
    {                                                                           \
        ASSERT(type_ == QUEUE                                                   \
               || ((fn_ && !fnWithCtx_) || (!fn_ && fnWithCtx_)));              \
        ASSERT(type_ != QUEUE || ((q_ && !qWithCtx_) || (!q_ && qWithCtx_)));   \
                                                                                \
        if (type_ == INTERRUPT) {                                               \
            fn_ ? (h_->*fn_)(TARG(t,n)) : (h_->*fnWithCtx_)(TARG(t,n), ctx_);   \
        } else if (type_ == ASYNCSCHEDULE) {                                    \
            fn_ ? ThreadPool::Schedule(h_, fn_, TARG(t,n))                      \
                : ThreadPool::Schedule(h_, fnWithCtx_, TARG(t,n), ctx_);        \
        } else if (type_ == QUEUE) {                                            \
            q_ ? q_->Wakeup(TARG(t,n)) : qWithCtx_->Wakeup(TARG(t,n), ctx_);    \
        } else {                                                                \
            DEADEND                                                             \
        }                                                                       \
    }                                                                           \
                                                                                \
    template<class X>                                                           \
    void SetCtx(const X & x)                                                    \
    {                                                                           \
        ctx_ = static_cast<uintptr_t>(x);                                       \
    }                                                                           \
                                                                                \
    template<class X>                                                           \
    X GetCtx() const                                                            \
    {                                                                           \
        return static_cast<X>(ctx_);                                            \
    }                                                                           \
                                                                                \
private:                                                                        \
                                                                                \
    cbtype_t type_;                                                             \
    CHandle * h_;                                                               \
    void (CHandle::*fn_)(TPARAM(T,t,n));                                        \
    void (CHandle::*fnWithCtx_)(TPARAM(T,t,n), uintptr_t);                      \
    CompletionQueue##TSUFFIX<TENUM(T,n)> * q_;                                  \
    CompletionQueueWithCtx##TSUFFIX<TENUM(T,n), uintptr_t> * qWithCtx_;         \
    uintptr_t ctx_;                                                             \
};                                                                              \
                                                                                \
template<TDEF(T,n)>                                                             \
using CHandler##TSUFFIX = CompletionHandler##TSUFFIX<TENUM(T,n)>;               \
                                                                                \
template<class _OBJ_, TDEF(T,n)>                                                \
CompletionHandler##TSUFFIX<TENUM(T,n)>                                          \
intr_fn(_OBJ_ * h, void (_OBJ_::*fn)(TENUM(T,n)))                               \
{                                                                               \
    CompletionHandler##TSUFFIX<TENUM(T,n)> ch(                                  \
                CompletionHandler##TSUFFIX<TENUM(T,n)>::INTERRUPT,              \
                (CHandle *) h, (void (CHandle::*)(TENUM(T,n))) fn);             \
    return ch;                                                                  \
}                                                                               \
                                                                                \
template<class _OBJ_, TDEF(T,n), class TCTX>                                    \
CompletionHandler##TSUFFIX<TENUM(T,n)>                                          \
intr_fn(_OBJ_ * h, void (_OBJ_::*fn)(TENUM(T,n), TCTX), TCTX ctx)               \
{                                                                               \
    CompletionHandler##TSUFFIX<TENUM(T,n)> ch(                                  \
                CompletionHandler##TSUFFIX<TENUM(T,n)>::INTERRUPT,              \
                (CHandle *) h, (void (CHandle::*)(TENUM(T,n), uint64_t)) fn,    \
                ctx);                                                           \
    return ch;                                                                  \
}                                                                               \
                                                                                \
template<class _OBJ_, TDEF(T,n)>                                                \
CompletionHandler##TSUFFIX<TENUM(T,n)>                                          \
async_fn(_OBJ_ * h, void (_OBJ_::*fn)(TENUM(T,n)))                              \
{                                                                               \
    CompletionHandler##TSUFFIX<TENUM(T,n)> ch(                                  \
        CompletionHandler##TSUFFIX<TENUM(T,n)>::ASYNCSCHEDULE, (CHandle *) h,   \
        (void (CHandle::*)(TENUM(T,n))) fn);                                    \
    return ch;                                                                  \
}                                                                               \
                                                                                \
template<class _OBJ_, TDEF(T,n), class TCTX>                                    \
CompletionHandler##TSUFFIX<TENUM(T,n)>                                          \
async_fn(_OBJ_ * h, void (_OBJ_::*fn)(TENUM(T,n), TCTX), TCTX ctx)              \
{                                                                               \
    CompletionHandler##TSUFFIX<TENUM(T,n)> ch(                                  \
        CompletionHandler##TSUFFIX<TENUM(T,n)>::ASYNCSCHEDULE, (CHandle *) h,   \
        (void (CHandle::*)(TENUM(T,n), uintptr_t)) fn,                          \
        reinterpret_cast<uintptr_t>(ctx));                                      \
    return ch;                                                                  \
}                                                                               \

COMPLETION_HANDLER(,1)  // CompletionHandler<T>
COMPLETION_HANDLER(2,2) // CompletionHandler2<T1,T2>
COMPLETION_HANDLER(3,3) // CompletionHandler3<T1,T2,T3>

#define ASYNCFN(n)                                                              \
template<class _OBJ_, TDEF(T,n)>                                                \
void (CHandle::*async_fn(void (_OBJ_::*fn)(TENUM(T,n))))(TENUM(T,n))            \
{                                                                               \
    typedef void (CHandle::*toptr_t)(TENUM(T,n));                               \
    return (toptr_t) fn;                                                        \
}

ASYNCFN(1)  // async_fn<T>(fn)
ASYNCFN(2)  // async_fn<T1,T2>(fn)
ASYNCFN(3)  // async_fn<T1,T2,T3>(fn)

} // namespace dh_core

#endif // _DH_CORE_ASYNC_H_
