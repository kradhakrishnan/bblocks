#ifndef _CORE_INLIST_H_
#define _CORE_INLIST_H_

#include "core/util.hpp"

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

private:

    T * head_; // pop
    T * tail_; // push
};

}

#endif
