#ifndef _DH_CORE_FN_H_
#define _DH_CORE_FN_H_

#include "core/thread-pool.h"

#define BADPTR 0xbadbada

namespace dh_core {

template<class T>
class Raw
{
public:

    Raw(T * t = NULL) : t_(t)
    {
    }

    ~Raw()
    {
        t_ = (T *) BADPTR;
    }

    T * operator->() const
    {
        ASSERT(t_);
        return t_;
    }

    T * Get() const
    {
        return t_;
    }

    void Delete()
    {
        ASSERT(t_);
        delete t_;
        t_ = (T *) BADPTR;
    }

private:

    T * t_;
};

/**
 */
class GenericFn : public ThreadRoutine
{
public:

    virtual void Invoke() = 0;
};

/**
 */
template<class _A1_, class _A2_>
class GenericFn2 : public GenericFn
{
public:

    virtual void Set(_A1_ & a1, _A2_ & a2) = 0;
};

/**
 */
template<class _OBJ_, class _A1_, class _A2_>
class MemberFn2 : public GenericFn
{
public:

    virtual void Set(const _A1_ & a1, const _A2_ & a2)
    {
        a1_ = a1;
        a2_ = a2;
    }

    virtual void Invoke()
    {
        (*obj_.*fn_)(a1_, a2_);
    }

    virtual void Run()
    {
        (*obj_.*fn_)(a1_, a2_);
    }

private:

    _OBJ_ * obj_;
    void (_OBJ_::*fn_)(_A1_ & a1, _A2_ & a2);
    _A1_ a1_;
    _A2_ a2_;
};

template<class _OBJ_, class _A1_, class _A2_>
MemberFn2<_OBJ_, _A1_, _A2_> *
fn2(_OBJ_ * obj)
{
    return new MemberFn2<_OBJ_, _A1_, _A2_>(obj);
}

}

#endif
