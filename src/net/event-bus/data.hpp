#pragma once

#include "util.h"
#include "buf/buffer.h"

namespace bblocks
{

// .............................................................................. Serializeable ....

struct Serializeable
{
	virtual ~Serializeable() {}

	/**
	 *
	 */
	virtual void Encode(IOBuffer & buf, size_t & pos) = 0;

	virtual void Encode(IOBuffer & buf)
	{
		size_t pos = 0;
		Encode(buf, pos);
	}

	/**
	 *
	 */
	virtual void Decode(IOBuffer & buf, size_t & pos) = 0;
	virtual void Decode(IOBuffer & buf)
	{
		size_t pos = 0;
		Decode(buf, pos);
	}

	virtual size_t Size() const = 0;
};

// ..................................................................................... Int<T> ....

template<class T>
struct Int : Serializeable
{
	explicit Int(const T v = 0) : v_(v) {}

	virtual void Encode(IOBuffer & buf, size_t & pos)
	{
		buf.UpdateInt(v_, pos);
	}

	virtual void Decode(IOBuffer & buf, size_t & pos)
	{
		buf.ReadInt(v_, pos);
	}

	bool operator==(const Int<T> & rhs) const { return v_ == rhs.v_; }
	bool operator==(const T & t) const { return t == v_; }
	Int<T> & operator=(const T & v) { v_ = v; return *this; }
	virtual size_t Size() const { return sizeof(v_); }
	void Set(const uint32_t v) { v_ = v; }
	const T & Get() const { return v_; }

	T v_;
};

// ............................................................................... Int<uint8_t> ....

template<>
struct Int<uint8_t> : Serializeable
{
	explicit Int<uint8_t>(const uint8_t v = 0) : v_(v) {}

	virtual void Encode(IOBuffer & buf, size_t & pos)
	{
		buf.Update(v_, pos);
	}

	virtual void Decode(IOBuffer & buf, size_t & pos)
	{
		buf.Read(v_, pos);
	}

	bool operator==(const Int<uint8_t> & rhs) const { return v_ == rhs.v_; }
	bool operator==(const uint8_t & t) const { return t == v_; }
	virtual size_t Size() const { return sizeof(v_); }
	void Set(const uint32_t v) { v_ = v; }
	const uint8_t & Get() const { return v_; }

	uint8_t v_;
};

typedef Int<uint8_t> UInt8;
typedef Int<uint16_t> UInt16;
typedef Int<uint32_t> UInt32;
typedef Int<uint64_t> UInt64;

// ................................................................................... Raw<int> ....

template<int SIZE>
struct Raw : Serializeable
{
	explicit Raw(const uint8_t v[] = NULL)
	{
		if (v)
			memcpy(v_, v, SIZE);
		else
			memset(v_, /*ch=*/ 0, SIZE);
	}

	virtual void Encode(IOBuffer & buf, size_t & pos)
	{
		buf.Update(v_, pos);
	}

	virtual void Decode(IOBuffer & buf, size_t & pos)
	{
		buf.Read(v_, pos);
	}

	virtual size_t Size() const { return sizeof(v_); }
	const uint8_t * & Get() const { return v_; }
	void Set(const uint8_t v[]) { memcpy(v_, v, SIZE); }

	bool operator==(const uint8_t v[]) const
	{
		return memcmp(v_, v, SIZE) == 0;
	}

	uint8_t v_[SIZE];
};

// ..................................................................................... String ....

struct String : Serializeable
{
	explicit String(const string & v = string()) : v_(v) {}

	virtual void Encode(IOBuffer & buf, size_t & pos);
	virtual void Decode(IOBuffer & buf, size_t & pos);

	virtual size_t Size() const 
	{
		return v_.size() + sizeof(uint32_t);
	}

	bool operator==(const string & str) const { return str == v_; }
	bool operator==(const String & rhs) const { return v_ == rhs.v_; }
	void Set(const string & v) { v_ = v; }
	const string & Get() const { return v_; }

	string v_;
};

// .................................................................................... List<T> ....

template<class T>
struct List : Serializeable
{
	List(const vector<T> & v = vector<T>()) : v_(v) {}

	void
	Encode(IOBuffer & buf, size_t & pos)
	{
		buf.UpdateInt<uint32_t>(v_.size(), pos);
		for (size_t i = 0; i < v_.size(); ++i) {
			v_[i].Encode(buf, pos);
		}
	}

	void
	Decode(IOBuffer & buf, size_t & pos)
	{
		uint32_t size;
		buf.ReadInt(size, pos);

		v_.resize(size);

		for (size_t i = 0; i < size; ++i) {
			v_[i].Decode(buf, pos);
		}
	}

	virtual size_t Size() const
	{
		return (v_.size() * sizeof(T)) + sizeof(uint32_t);
	}

	bool operator==(const List<T> & rhs) const
	{
		return v_ == rhs.v_;
	}

	void Set(const vector<T> & v) { v_ = v; }
	void Set(const List<T> & v) { v_ = v.v_; }
	const vector<T> & Get() const { return v_; }

	vector<T> v_;
};

}
