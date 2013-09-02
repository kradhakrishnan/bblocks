#ifndef _CORE_NET_RPC_H_
#define _CORE_NET_RPC_H_

#include "buffer.h"
#include "util.hpp"

namespace dh_core
{

// .................................................................................... RPCData ....

struct RPCData
{
	virtual void Encode(IOBuffer & buf, size_t & pos) = 0;
	virtual void Decode(IOBuffer & buf, size_t & pos) = 0;
	virtual size_t Size() const = 0;
};

// ..................................................................................... Int<T> ....

template<class T>
struct Int : RPCData
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
	virtual size_t Size() const { return sizeof(v_); }
	void Set(const uint32_t v) { v_ = v; }
	const T & Get() const { return v_; }

	T v_;
};

// ............................................................................... Int<uint8_t> ....

template<>
struct Int<uint8_t> : RPCData
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
struct Raw : RPCData
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

struct String : RPCData
{
	explicit String(const std::string & v = std::string()) : v_(v) {}

	virtual void Encode(IOBuffer & buf, size_t & pos);
	virtual void Decode(IOBuffer & buf, size_t & pos);

	virtual size_t Size() const 
	{
		return v_.size() + sizeof(uint32_t);
	}

	bool operator==(const std::string & str) const { return str == v_; }
	bool operator==(const String & rhs) const { return v_ == rhs.v_; }
	void Set(const std::string & v) { v_ = v; }
	const std::string & Get() const { return v_; }

	std::string v_;
};

// .................................................................................... List<T> ....

template<class T>
struct List : RPCData
{
	List(const std::vector<T> & v = std::vector<T>()) : v_(v) {}

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

	void Set(const std::vector<T> & v) { v_ = v; }
	void Set(const List<T> & v) { v_ = v.v_; }
	const std::vector<T> & Get() const { return v_; }

	std::vector<T> v_;
};

// .................................................................................. RPCPacket ....

struct RPCPacket : RPCData
{
	RPCPacket(const uint8_t opcode, const uint8_t opver = 0)
		: opcode_(opcode), opver_(opver), size_(Size()), cksum_(0)
	{}

	virtual void Encode(IOBuffer & buf, size_t & pos)
	{
		INVARIANT(buf.Size() >= Size());

		opcode_.Encode(buf, pos);
		opver_.Encode(buf, pos);
		size_.Encode(buf, pos);
		cksum_.Encode(buf, pos);
	}

	virtual void Encode(IOBuffer & buf) = 0;

	virtual void Decode(IOBuffer & buf, size_t & pos)
	{
		INVARIANT(buf.Size() >= Size());

		opcode_.Decode(buf, pos);
		opver_.Decode(buf, pos);
		size_.Decode(buf, pos);
		cksum_.Decode(buf, pos);
	}

	virtual void Decode(IOBuffer & buf) = 0;

	virtual size_t Size() const
	{
		return opcode_.Size()
			+ opver_.Size()
			+ size_.Size()
			+ cksum_.Size();
	}

	void EncodePacketHash(IOBuffer & buf)
	{
		INVARIANT(cksum_.Get() == 0);

		const size_t off = opcode_.Size() + opver_.Size() + size_.Size();

		cksum_.Set(Adler32::Calc(buf.Ptr(), Size()));
		buf.UpdateInt(cksum_.Get(), off);
	}

	bool IsPacketValid(IOBuffer & buf)
	{
		INVARIANT(buf.Size() >= Size());

		const size_t off = opcode_.Size() + opver_.Size() + size_.Size();

		/* 
		 * Fetch checksum from buffer
		 */
		uint32_t ecksum;
		buf.ReadInt(ecksum, off);
		INVARIANT(ecksum == cksum_.Get());

		/*
		 * calc checksum on the buffer
		 */
		buf.UpdateInt(/*val=*/ (uint32_t) 0, off);
		uint32_t acksum = Adler32::Calc(buf.Ptr(), Size());

		/*
		 * fix the buffer
		 */
		buf.UpdateInt(ecksum, off);

		return ecksum == acksum;
	}

	UInt8 opcode_;
	UInt8 opver_;
	UInt16 size_;
	UInt32 cksum_;
};

}

#endif
