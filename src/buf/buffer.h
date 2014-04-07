#ifndef _DH_CORE_BUFFER_H_
#define _DH_CORE_BUFFER_H_

#include <malloc.h>
#include <memory>
#include <arpa/inet.h>
#include <sstream>

#include "assert.h"
#include "atomic.h"

namespace dh_core {

//..................................................................................... IOBuffer ...

/**
 * @class IOBuffer
 *
 * This class provides the buffer handler requirement for IO processing. This is designed to support
 * all processing needs for both disk based systems and network based system. The class encapsulates
 * shared pointer and provides accessors for manipulating the buffer as a network packet or as data
 * fetched from the disk subsystem.
 */
class IOBuffer
{
public:

	/*
	 * Deallocator
	 *
	 * TODO: Templatize the code to work with any buffer manager
	 */
	struct Dalloc
	{
		void operator()(uint8_t * data)
		{
			::free(data);
		}
	};

	/*
	 * static methods
	 */
	static IOBuffer Alloc(const size_t size)
	{
		void * ptr;
		int status = posix_memalign(&ptr, 512, size);
		INVARIANT(status != -1);
		return IOBuffer(std::shared_ptr<uint8_t>((uint8_t *) ptr, Dalloc()), size);
	}

	/*
	 * Create/destroy
	 */
	IOBuffer() : size_(0), off_(0) {}
	virtual ~IOBuffer() {}

	uint8_t * operator->() { return data_.get() + off_; }
	operator bool() const { return data_.get(); }

	/*
	 * Generic helper
	 */
	uint8_t * Ptr()
	{
		ASSERT(data_.get());
		return data_.get();
	}

	size_t Size() const
	{
		return size_;
	}

	void Reset()
	{
		data_.reset();
		size_ = off_ = 0;
	}

	void Trash()
	{
		data_.reset();
	}

	IOBuffer Cut(const size_t size)
	{
		ASSERT(size < size_);

		off_ += size;
		size_ -= size;

		return IOBuffer(data_, size, off_);
	}

	void FillRandom()
	{
		for (uint32_t i = 0; i < size_; ++i) {
			data_.get()[off_ + i] = rand() % 255;
		}
	}

	void Fill(const uint8_t ch = 0)
	{
		memset(data_.get() + off_, ch, size_);
	}

	void Copy(uint8_t * src, const size_t size)
	{
		memcpy(data_.get(), src, size);
	}

	template<class T>
	void Copy(const T & t)
	{
		INVARIANT(sizeof(t) <= size_);
		memcpy(data_.get(), (uint8_t *) &t, sizeof(T));
	}

	/*
	 * Serializing/de-serializing helpers
	 */
	template<class T>
	void Update(const T & t, size_t & pos)
	{
		INVARIANT(sizeof(T) <= (size_ - pos));
		memcpy(data_.get() + off_ + pos, (uint8_t *) &t, sizeof(T));

		pos += sizeof(T);
	}

	template<class T>
	void Update(const T & t, const size_t & pos)
	{
		size_t tmp = pos;
		Update(t, tmp);
	}

	template<class T>
	void UpdateInt(const T & t, size_t & pos)
	{
		INVARIANT(sizeof(T) % 2 == 0);
		INVARIANT(sizeof(T) <= (size_ - pos));

		T v = t;
		for (uint32_t i = 0; i < (sizeof(T) / 2); ++i) {
			uint16_t * p = (uint16_t *)(data_.get() + off_ + pos);
			*p = htons((uint16_t) v);
			v = v >> 16;
			pos += 2;
		}
	}

	template<class T>
	void UpdateInt(const T & t, const size_t & pos)
	{
		size_t tmp = pos;
		UpdateInt(t, tmp);
	}

	template<class T>
	void Read(T & t, size_t & pos) const
	{
		INVARIANT(pos + sizeof(T) <= size_);

		memcpy(&t, data_.get() + off_ + pos, sizeof(T));
		pos += sizeof(T);
	}

	template<class T>
	void Read(T & t, const size_t & pos) const
	{
		size_t tmp = pos;
		Read(t, tmp);
	}

	template<class T>
	void ReadInt(T & t, size_t & pos)
	{
		INVARIANT(sizeof(t) % 2 == 0);

		t = 0;
		for (size_t i = 0; i < (sizeof(T) / 2); ++i) {
			uint16_t * p = (uint16_t *)(data_.get() + off_ + pos);
			t += ntohs(*p) << (i * 16);
			pos += 2;
		}
	}

	template<class T>
	void ReadInt(T & t, const size_t & pos)
	{
		size_t tmp = pos;
		ReadInt(t, tmp);
	}

	std::string Dump() const
	{
		if (!data_) return std::string();

		std::ostringstream ss;

		ss << "[";
		for (size_t i = 0; i < size_; i++) {
			char ch = data_.get()[off_ + i];
			if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z')
			    || (ch >= '0' && ch <= '9')) {
				ss << ch << ".";
			} else {
				ss << (int) ch << ".";
			}
		}
		ss << "]";

		return ss.str();
	}

protected:

	IOBuffer(const std::shared_ptr<uint8_t> & data, const size_t size, const size_t off = 0)
		: data_(data), size_(size), off_(off)
	{}

	std::shared_ptr<uint8_t> data_;
	size_t size_;
	size_t off_;
};

}

#endif
