#pragma once

#include <inttypes.h>
#include <list>

#include "schd/thread-ctx.h"
#include "util.hpp"
#include "lock.h"

namespace bblocks {

//.................................................................................. BufferPool ....

class BufferPool : public Singleton<BufferPool>
{
public:

	template<class T>
	static void * Alloc()
	{
		INVARIANT(ThreadCtx::pool_);

		void * ptr = NULL;
		const size_t size = Math::Roundup(sizeof(T), 512);
		const size_t id = (size / 512) - 1;

		if (id >= SLAB_DEPTH || ThreadCtx::pool_[id].empty()) {
			int status = posix_memalign(&ptr, 512, size);
			INVARIANT(status != -1);
			return ptr;
		}

		uint8_t * data = ThreadCtx::pool_[id].front();
		ThreadCtx::pool_[id].pop_front();
		return data;
	}

	template<class T>
	static void Dalloc(T * t)
	{
		INVARIANT(ThreadCtx::pool_);

		const size_t size = Math::Roundup(sizeof(T), 512);
		const size_t id = (size / 512) - 1;

		if (id >= SLAB_DEPTH) {
			::free((void *) t);
			return;
		}

		ThreadCtx::pool_[id].push_back((uint8_t *) t);
	}
};

// ........................................................................ BufferPoolObject<T> ....

template<class T>
class BufferPoolObject
{
public:

	void * operator new(size_t size, void * ptr)
	{
		ASSERT(size == sizeof(T));
		return ptr;
	}

	void operator delete(void * ptr)
	{
		BufferPool::Dalloc<T>((T *) ptr);
	}

protected:

	void * operator new(size_t size);
};

// ............................................................................. AutoRelease<T> ....

template<class T>
class AutoRelease
{
public:

	explicit AutoRelease(T * t) : t_(t) {}
	~AutoRelease()
	{
		if (t_) {
			BufferPool::Dalloc<T>(t_);
		}
	}

private:

	AutoRelease();
	AutoRelease(const AutoRelease &);
	AutoRelease<T> & operator=(const AutoRelease<T> &);

	T * t_;
};

}
