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

#define COMPLETION_HANDLER(TSUFFIX,TDEF,TENUM,TPARAM,TARG) \
template<TDEF> \
class CompletionHandler##TSUFFIX \
{ \
public: \
\
    typedef enum \
    { \
        INTERRUPT = 0, \
        ASYNCSCHEDULE, \
    } \
    cbtype_t; \
\
    CompletionHandler##TSUFFIX(cbtype_t type, CHandle * h, \
                               void (CHandle::*fn)(TPARAM)) \
        : type_(type), h_(h), fn_(fn), fnWithCtx_(NULL), ctx_(0) \
    {} \
\
    CompletionHandler##TSUFFIX(cbtype_t type, CHandle * h, \
                               void (CHandle::*fnWithCtx)(TPARAM, uintptr_t), \
                               uintptr_t ctx) \
        : type_(type), h_(h), fn_(NULL), fnWithCtx_(fnWithCtx), ctx_(ctx) \
    {} \
\
    void Interrupt(TPARAM) \
    { \
        INVARIANT(type_ == INTERRUPT); \
        (h_->*fn_)(TARG); \
    } \
\
    void Wakeup(TPARAM) \
    { \
        ASSERT((!(fn_ && fnWithCtx_) && (fn_ || fnWithCtx_))); \
\
        if (type_ == INTERRUPT) { \
            fn_ ? (h_->*fn_)(TARG) : (h_->*fnWithCtx_)(TARG, ctx_); \
        } else { \
            fn_ ? ThreadPool::Schedule(h_, fn_, TARG) \
                : ThreadPool::Schedule(h_, fnWithCtx_, TARG, ctx_); \
        } \
    } \
\
    template<class X> \
    void SetCtx(const X & x) \
    { \
        ctx_ = static_cast<uintptr_t>(x); \
    } \
\
    template<class X> \
    X GetCtx() const \
    { \
        return static_cast<X>(ctx_); \
    } \
\
private: \
\
    cbtype_t type_; \
    CHandle * h_; \
    void (CHandle::*fn_)(TPARAM); \
    void (CHandle::*fnWithCtx_)(TPARAM, uintptr_t); \
    uintptr_t ctx_; \
}; \
\
template<class _OBJ_, TDEF> \
CompletionHandler##TSUFFIX<TENUM> \
intr_fn(_OBJ_ * h, void (_OBJ_::*fn)(TENUM)) \
{ \
    CompletionHandler##TSUFFIX<TENUM> ch( \
                CompletionHandler##TSUFFIX<TENUM>::INTERRUPT, \
                (CHandle *) h, (void (CHandle::*)(TENUM)) fn); \
    return ch; \
} \
\
template<class _OBJ_, TDEF, class TCTX> \
CompletionHandler##TSUFFIX<TENUM> \
intr_fn(_OBJ_ * h, void (_OBJ_::*fn)(TENUM, TCTX), const TCTX ctx) \
{ \
    CompletionHandler##TSUFFIX<TENUM> ch( \
                CompletionHandler##TSUFFIX<TENUM>::INTERRUPT, \
                (CHandle *) h, (void (CHandle::*)(TENUM, uint64_t)) fn, ctx); \
    return ch; \
} \
\
template<class _OBJ_, TDEF> \
CompletionHandler##TSUFFIX<TENUM> \
async_fn(_OBJ_ * h, void (_OBJ_::*fn)(TENUM)) \
{ \
    CompletionHandler##TSUFFIX<TENUM> ch( \
        CompletionHandler##TSUFFIX<TENUM>::ASYNCSCHEDULE, (CHandle *) h, \
        (void (CHandle::*)(TENUM)) fn); \
    return ch; \
} \
\
template<class _OBJ_, TDEF, class TCTX> \
CompletionHandler##TSUFFIX<TENUM> \
async_fn(_OBJ_ * h, void (_OBJ_::*fn)(TENUM, TCTX), TCTX ctx) \
{ \
    CompletionHandler##TSUFFIX<TENUM> ch( \
        CompletionHandler##TSUFFIX<TENUM>::ASYNCSCHEDULE, (CHandle *) h, \
        (void (CHandle::*)(TENUM, uintptr_t)) fn, \
        reinterpret_cast<uintptr_t>(ctx)); \
    return ch; \
}


COMPLETION_HANDLER(, // TSUFFIX
                   class T, // TDEF
                   T,       // TENUM
                   const T t,   // TPARAM
                   t)       // TARG

COMPLETION_HANDLER(2, // TSUFFIX
                   class P1 COMMA class P2, // TDEF
                   P1 COMMA P2,       // TENUM
                   const P1 p1 COMMA const P2 p2,   // TPARAM
                   p1 COMMA p2)       // TARG


#define ASYNCFN(A,B) \
template<class _OBJ_, A> \
void (CHandle::*async_fn(void (_OBJ_::*fn)(B)))(B) \
{ \
    typedef void (CHandle::*toptr_t)(B); \
    return (toptr_t) fn; \
}

ASYNCFN(class _P1_, _P1_)
ASYNCFN(class _P1_ COMMA class _P2_, _P1_ COMMA _P2_)
ASYNCFN(class _P1_ COMMA class _P2_ COMMA class _P3_, _P1_ COMMA _P2_ COMMA _P3_)

} // namespace dh_core

#endif // _DH_CORE_ASYNC_H_
