#ifndef _DH_CORE_ASYNC_H_
#define _DH_CORE_ASYNC_H_

#include "core/defs.h"
#include "core/buffer.h"

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

// TODO
// Need to think how to fit this into the structure

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
