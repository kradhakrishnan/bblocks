#ifndef _IOCORE_UTIL_H_
#define _IOCORE_UTIL_H_

#include <list>
#include <queue>
#include <sstream>
#include <memory>
#include <inttypes.h>
#include <rpc/xdr.h>
#include <zlib.h>

#include <tr1/memory>

#include "defs.h"
#include "atomic.h"

namespace dh_core {

typedef int fd_t;
typedef int status_t;

enum
{
	OK = 0,
	FAIL = -1
};

#if defined(__i386__)
static inline unsigned long long rdtsc(void)
{
	unsigned long long int x;
	__asm__ volatile (".byte 0x0f, 0x31" : "=A" (x));
	return x;
}
#elif defined(__x86_64__)
static inline unsigned long long rdtsc(void)
{
	unsigned hi, lo;
	__asm__ __volatile__ ("rdtsc" : "=a"(lo), "=d"(hi));
	return ( (unsigned long long)lo)|( ((unsigned long long)hi)<<32 );
}
#endif


template<class T> using SharedPtr = std::shared_ptr<T>;

template<class T>
SharedPtr<T>
MakeSharedPtr(T * t)
{
	return SharedPtr<T>(t);
}

//........................................................................................ Math ....

class Math
{
public:

	inline static size_t Roundup(const size_t n, const size_t s)
	{
		INVARIANT(s);
		return ((n / s) + (n % s ? 1 : 0)) * s;
	}
};

//........................................................................................ Time ....

class Time
{
public:

	static uint64_t NowInMilliSec()
	{
		timeval tv;
		int status = gettimeofday(&tv, /*tz=*/ NULL);
		(void) status;
		ASSERT(!status);
		return tv.tv_sec * 1000 + (tv.tv_usec / 1000);
	}
};

//..................................................................................... Adler32 ....

/**
 * Adler32 checksume wrapper.
 */
class Adler32
{
public:

	static uint32_t Calc(uint8_t * data, const uint32_t size)
	{
		return adler32(/*cksum=*/ 0, data, size);
	}

	Adler32() : cksum_(0) {}

	void Update(uint8_t * data, const uint32_t size)
	{
		cksum_ = adler32(cksum_, data, size);
	}

	template<class T>
	void Update(const T & t)
	{
		cksum_ = adler32(cksum_, (uint8_t *) &t, sizeof(T));
	}

	uint32_t Hash() const { return cksum_; }
	void Reset() { cksum_ = 0; }

private:

	uint32_t cksum_;
};

//............................................................................. StateMachine<T> ....

/**
 * A generic state machine implementation for tracking states. Typically you
 * would pass an enum as T.
 *
 */
template<class T>
class StateMachine
{
public:

	StateMachine(const T & state) : state_(state) {}

	bool MoveTo(const T & to, const unsigned int & from)
	{
		ASSERT(from);

		if (state_ & from) {
			state_ = to;
			return true;
		}

		return false;
	}

	T MoveTo(const T & to)
	{
		ASSERT(!state_);
		ASSERT(to);

		T old = state_;
		state_ = to;
		return old;
	}

	bool Is(const unsigned & states) const
	{
		ASSERT(states);
		return state_ & states;
	}

	bool operator==(const T & rhs)
	{
		return state_ == rhs;
	}

	const T & state() const
	{
		return state_;
	}

private:

	T state_;
};

//................................................................................ Singleton<T> ....

/**
 * A generic implementation of singleton design pattern
 *
 * TODO:
 * - Implement check and lock pattern and make it thread safe (refer to
 * architectural design pattern)
 *
 */
template<class T>
class Singleton
{
public:

	static void Init()
	{
		ASSERT(!Singleton<T>::instance_);
		Singleton<T>::instance_ = new T();
	}

	static T & Instance()
	{
		ASSERT(Singleton<T>::instance_);
		return *instance_;
	}

	static void Destroy()
	{
		ASSERT(Singleton<T>::instance_);
		delete Singleton<T>::instance_;
		Singleton<T>::instance_ = NULL;
	}

private:

	static T * instance_;
};

template<class T>
T * Singleton<T>::instance_ = NULL;

//................................................................................ BoundedQueue ....

template<class T>
class BoundedQ
{
public:

	BoundedQ(const size_t capacity)
	{
		q_.reserve(capacity);
	}

	void Push(const T & t)
	{
		q_.push_back(t);
	}

	bool IsEmpty() const
	{
		return q_.empty();
	}

	size_t Size() const
	{
		return q_.size();
	}

	T Pop()
	{
		T t = q_.front();
		// pop front
		q_.erase(q_.begin());
		return t;
	}

	T & Front()
	{
		return q_.front();
	}

	typename std::vector<T>::iterator Begin()
	{
		return q_.begin();
	}

	typename std::vector<T>::iterator End()
	{
		return q_.end();
	}

	void Clear()
	{
		q_.clear();
	}

private:

	std::vector<T> q_;
};

};

#endif
