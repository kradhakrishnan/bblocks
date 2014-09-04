#ifndef _CORE_INLIST_H_
#define _CORE_INLIST_H_

#include "util.hpp"
#include "lock.h"


namespace bblocks {

//............................................................................ InListElement<T> ....

/**
 * Base class for an element which will be inserted into inlists.
 *
 * Usage : class X : public InListElement<X> {}
 */
template<class T>
struct InListElement
{
	InListElement() : next_(NULL), prev_(NULL) {}

	T * next_;
	T * prev_;
};

//................................................................................... Inlist<T> ....

/**
 * Generic implementation of inlist. Provides typical list functionality with
 * the overhead of allocation and deallocation of elements. Elements are
 * linked-in and unlinked as per the operation.
 *
 * The elements are expected to fields next_ and prev_ defined. Typically you
 * would extent InListElement<T>
 */
template<class T>
class InList
{
public:

	InList() : head_(NULL), tail_(NULL) {}

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
		if (head_) head_->prev_ = t;

		head_ = t;
		if (!tail_) tail_ = head_;
	}

	inline void Unlink(T * t)
	{
		ASSERT(t);
		ASSERT(head_ && tail_);
		ASSERT(t->prev_ || head_ == t);
		ASSERT(t->next_ || tail_ == t);

		if (t->prev_) t->prev_->next_ = t->next_;
		if (t->next_) t->next_->prev_ = t->prev_;

		if (tail_ == t) tail_ = t->prev_;
		if (head_ == t) head_ = t->next_;

		t->next_ = t->prev_ = NULL;
	}

	inline T * Pop()
	{
		ASSERT(tail_ && head_);
		ASSERT(!tail_->next_);
		ASSERT(!head_->prev_);

		T * t = tail_;

		tail_ = tail_->prev_;
		if (tail_) tail_->next_ = NULL;

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

// ................................................................................. InQueue<T> ....

/**
 * In list used as queue. Provides all the same benefits and requirements of
 * inlist, but provide the interface for a queue.
 *
 * This is meant to be a fast queue, so we employ adaptive spinning.
 */
template<class T>
class InQueue
{
public:

	static const unsigned int MAX_SPIN = 10000;

	InQueue(const string & name) : log_("/q/" + name), maxSpin_(1000) {}

	inline void Push(T * t)
	{
		lock_.Lock();
		q_.Push(t);
		lock_.Unlock();

		conditionEmpty_.Signal();
	}

	inline T * Pop()
	{
		T * t = TryPop();

		/*
		 * Return T if the pop was successful.
		 */
		if (t) return t;

		/*
		 * Timeout.
		 *
		 * No objects were received while spinning. Apply traditional lock and wait
		 * algorithm
		 */
		lock_.Lock();

		while (q_.IsEmpty()) {
			conditionEmpty_.Wait(&lock_);
		}

		t = q_.Pop();

		lock_.Unlock();

		return t;
	}

	inline T * Pop(const uint32_t ms)
	{
		T * t = TryPop();

		/*
		 * Return T if the pop was successful.
		 */
		if (t) return t;

		/*
		 * Timeout.
		 *
		 * No objects were received while spinning. Apply traditional lock and wait
		 * algorithm
		 */
		if (!lock_.Lock(ms))
			/*
			 * Timeout
			 */
			return NULL;

		while (q_.IsEmpty()) {
			if (!conditionEmpty_.Wait(&lock_, ms)) {
				/*
				 * Timeout waiting for object
				 */
				lock_.Unlock();
				return NULL;
			}
		}

		t = q_.Pop();

		lock_.Unlock();

		return t;
	}

	inline bool IsEmpty() const
	{
		lock_.Lock();
		const bool ret = q_.IsEmpty();
		lock_.Unlock();

		return ret;
	}

private:

	inline T * TryPop()
	{
		/*
		 * Spin for a little bit waiting for message. This increases the throughput rate on
		 * a loaded system since the cost of sleeping and waking is fairly high for a job
		 * scheduler algorithm
		 */
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

		return NULL;
    }

	InQueue();

	string log_;
	mutable PThreadMutex lock_;
	WaitCondition conditionEmpty_;
	InList<T> q_;
	unsigned int maxSpin_;
};

// ............................................................... Queue<T> ....

/**
 * Typical thread safe queue which uses blocking lock. You would use this queue
 * for general purpose programming.
 */
template<class T>
class Queue
{
public:

    Queue(const string & name)
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

    string log_;
    PThreadMutex lock_;
    WaitCondition conditionEmpty_;
    queue<T> q_;
};


}

#endif
