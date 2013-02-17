#ifndef _CORE_INLIST_H_
#define _CORE_INLIST_H_

#include "core/util.hpp"
#include "core/lock.h"

namespace dh_core {

template<class T>
struct InListElement
{
    InListElement()
        : next_(NULL)
        , prev_(NULL)
    {}

    T * next_;
    T * prev_;
};

template<class T>
class InList
{
public:

    InList()
        : head_(NULL)
        , tail_(NULL)
    {}

    virtual ~InList()
    {
        INVARIANT(!head_);
        INVARIANT(!tail_);
    }

    void Push(T * t)
    {
        INVARIANT(t);
        INVARIANT(t->next_);
        INVARIANT(t->prev_);

        t->next_ = head_ ? head_->next_ : NULL;
        head_ = t;

        if (!tail_) {
            tail_ = head_;
        }
    }

    T * Pop()
    {
        INVARIANT(tail_);

        T * t = tail_;
        Unlink(t);

        return t;
    }

    void Unlink(T * t)
    {
        INVARIANT(t);
        INVARIANT(t->next_ || t->prev_);

        if (t->prev_) {
            t->prev_->next_ = t->next_;
        }

        if (t->next_) {
            t->next_->prev_ = t->prev_;
        }

        t->next_ = t->prev_ = NULL;

        if (t == head_) {
            head_ = head_->next_;
        }

        if (t == tail_) {
            tail_ = tail_->prev_;
        }
    }

    bool IsEmpty() const
    {
        return !head_ && !tail_;
    }

private:

    T * head_; // pop
    T * tail_; // push
};

/**
 *
 */
template<class T>
class InQueue
{
public:

    InQueue(const std::string & name)
        : log_("/q/" + name)
        , lock_(new PThreadMutex())
    {}

    void Push(T * t)
    {
        AutoLock _(lock_.Get());

        q_.Push(t);
        conditionEmpty_.Signal();
    }

    T * Pop()
    {
        AutoLock _(lock_.Get());

        while (q_.IsEmpty()) {
            conditionEmpty_.Wait(lock_.Get());
        }

        return q_.Pop();
    }

private:

    LogPath log_;
    AutoPtr<PThreadMutex> lock_;
    WaitCondition conditionEmpty_;
    InList<T> q_;
};


}

#endif
