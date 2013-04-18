#ifndef _DH_CORE_FNPTR_H_
#define _DH_CORE_FNPTR_H_

#include <inttypes.h>
#include <boost/shared_ptr.hpp>

#include "core/thread-pool.h"

namespace dh_core {

template<class _A1_>
class Fn1 : public boost::enable_shared_from_this<Fn1<_A1_> >
{
public:

    virtual void operator()(_A1_ & a1) = 0;
};

template<class _A1_>
class Fn1ThreadRoutine : public ThreadRoutine
{
public:

    Fn1ThreadRoutine(const boost::shared_ptr<Fn1<_A1_> > & fn, const _A1_ & a1)
        : fn_(fn), a1_(a1)
    {
    }

    virtual void Run()
    {
        (*fn_)(a1_);
        delete this;
    }

    boost::shared_ptr<Fn1<_A1_> > fn_;
    _A1_ a1_;

};

template<class _OBJ_, class _A1_>
struct MemberFn1 : public Fn1<_A1_>
{
    typedef void (_OBJ_::*fn_t)(_A1_ &);

    MemberFn1(_OBJ_ * obj, fn_t fn)
        : obj_(obj), fn_(fn)
    {
    }

    virtual void operator()(_A1_ & a1)
    {
        (*obj_.*fn_)(a1);
    }


    _OBJ_ * obj_;
    fn_t fn_;
};

template<class _OBJ_, class _A1_>
boost::shared_ptr<MemberFn1<_OBJ_, _A1_> >
make_fnptr(_OBJ_ * obj, void (_OBJ_::*fn)(_A1_ &))
{
    return boost::shared_ptr<MemberFn1<_OBJ_, _A1_> >
                            (new MemberFn1<_OBJ_, _A1_>(obj, fn));
}

template<class _A1_>
Fn1ThreadRoutine<_A1_> *
make_th_routine(Fn1<_A1_> * fn, const _A1_ & a1)
{
    return new Fn1ThreadRoutine<_A1_>(fn, a1);
}

} // namespace dh_core

#endif
