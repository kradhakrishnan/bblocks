#ifndef _CORE_INLIST_H_
#define _CORE_INLIST_H_

#include "core/util.hpp"
#include "core/lock.h"

#define ALIGNED(x) __attribute__((aligned(sizeof(x))))

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

    inline void Push(T * t)
    {
        ASSERT((!head_ && !tail_) || (head_ && tail_));
        ASSERT(!head_ || !head_->prev_);
        ASSERT(!tail_ || !tail_->next_);

        ASSERT(t);
        ASSERT(!t->next_);
        ASSERT(!t->prev_);

        t->next_ = head_;
        if (head_) {
            head_->prev_ = t;
        }

        head_ = t;
        if (!tail_) {
            tail_ = head_;
        }
    }

    inline T * Pop()
    {
        ASSERT(tail_ && head_);
        ASSERT(!tail_->next_);
        ASSERT(!head_->prev_);

        T * t = tail_;

        tail_ = tail_->prev_;
        if (tail_) {
            tail_->next_ = NULL;
        }

        if (t == head_) {
            ASSERT(!tail_);
            head_ = NULL;
        }

        t->next_ = t->prev_ = NULL;

        return t;
    }

    inline bool IsEmpty() const
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

    static const unsigned int MAX_SPIN = 10000;

    InQueue(const std::string & name)
        : log_("/q/" + name)
        , maxSpin_(1000)
    {
    }

    inline void Push(T * t)
    {
        lock_.Lock();
        q_.Push(t);
        lock_.Unlock();

        conditionEmpty_.Signal();
    }

    inline T * Pop()
    {
        for (unsigned int i = 0; i < maxSpin_; ++i) {
            lock_.Lock();
            if (!q_.IsEmpty()) {
                T * t = q_.Pop();
                lock_.Unlock();
                return t;
            }
            lock_.Unlock();
            sched_yield();
        }

        // lame adaptive spinning
        // maxSpin_ = maxSpin_ * maxSpin_;
        // if (maxSpin_ > MAX_SPIN) {
        //    maxSpin_ = 10;
        // }

        lock_.Lock();
        while (q_.IsEmpty()) {
            conditionEmpty_.Wait(&lock_);
        }

        T * t = q_.Pop();
        lock_.Unlock();

        return t;
    }

private:

    InQueue();

    LogPath log_;
    PThreadMutex lock_;
    WaitCondition conditionEmpty_;
    InList<T> q_;
    unsigned int maxSpin_;
};


/**
 *
 */
template<class T>
class Queue
{
public:

    Queue(const std::string & name)
        : log_("/q/" + name)
    {
    }

    inline void Push(T * t)
    {
        lock_.Lock();
        q_.push(t);
        lock_.Unlock();

        conditionEmpty_.Signal();
    }

    inline T Pop()
    {
        lock_.Lock();
        while (q_.empty()) {
            conditionEmpty_.Wait(&lock_);
        }

        T t = q_.front();
        q_.pop();
        lock_.Unlock();

        return t;
    }

private:

    Queue();

    LogPath log_;
    PThreadMutex lock_;
    WaitCondition conditionEmpty_;
    std::queue<T> q_;
};


}

#endif
