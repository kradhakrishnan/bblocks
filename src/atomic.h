#ifndef _K_IOCORE_ATOMIC_H_
#define _K_IOCORE_ATOMIC_H_

#include <queue>
#include <boost/shared_ptr.hpp>
#include <inttypes.h>

#include "assert.h"

using __gnu_cxx::__atomic_add;
using __gnu_cxx::__exchange_and_add;

namespace dh_core {

class AtomicCounter
{
    public:

    AtomicCounter(_Atomic_word val = 0)
        : count_(val)
    {
    }

    const uint64_t Add(const int val)
    {
        return __exchange_and_add(&count_, val);
    }

    const uint64_t Count() const
    {
        return __exchange_and_add(&count_, /*val=*/ 0);
    }

    void Set(const uint64_t val)
    {
        while (!__sync_bool_compare_and_swap(&count_, Count(), val));
    }

    bool CompareAndSwap(const uint64_t val, const uint64_t prevVal)
    {
        return __sync_bool_compare_and_swap(&count_, prevVal, val);
    }

    private:

    mutable volatile _Atomic_word count_;

};

class AtomicFlag
{

public:

    AtomicFlag(const bool & flag)
        : flag_(flag)
    {
    }

    const bool Value() const
    {
        return flag_;
    }


    void Set(const bool & val)
    {
        while(!__sync_bool_compare_and_swap(&flag_, Value(), val));
    }


    void SetTrue()
    {
        while(!__sync_bool_compare_and_swap(&flag_, false, true));
    }

    bool TrySetTrue()
    {
        return __sync_bool_compare_and_swap(&flag_, false, true);
    }

    void SetFalse()
    {
        while(!__sync_bool_compare_and_swap(&flag_, true, false));
    }

    bool TrySetFalse()
    {
        return __sync_bool_compare_and_swap(&flag_, true, false);
    }


    operator bool()
    {
        return Value();
    }

private:

    mutable volatile _Atomic_word flag_;
};

class AutoAtomicFlag
{
public:

    AutoAtomicFlag(AtomicFlag * flag, const bool & val)
        : flag_(flag)
    {
        ASSERT(flag_);
        flag_->Set(val);
    }

    ~AutoAtomicFlag()
    {
        if (flag_) {
            flag_->Set(false);
        }
    }

private:

    AutoAtomicFlag();

    AtomicFlag * flag_;
};


};

#endif
