#pragma once

#include <list>

#include "net/event-bus/data.hpp"

namespace bblocks {

using namespace std;
using namespace bblocks;

// .................................................................................. NetPacket ....

class NetPacket : public Serializeable
{
public:

	void Add(Serializeable & var)
	{
		vars_.push_back(&var);
	}

	virtual void Encode(IOBuffer & buf, size_t & pos) override
	{
		for (auto it = vars_.begin(); it != vars_.end(); ++it) {
			(*it)->Encode(buf, pos);
		}
	}

	virtual void Decode(IOBuffer & buf, size_t & pos) override
	{
		for (auto it = vars_.begin(); it != vars_.end(); ++it) {
			(*it)->Decode(buf, pos);
		}
	}

	virtual size_t Size() const override
	{
		size_t size = 0;
		for (auto it = vars_.begin(); it != vars_.end(); ++it) {
			size += (*it)->Size();
		}

		return size;
	}

private:

    list<Serializeable *> vars_;
};

// ................................................................................ EventPacket ....

struct EventPacket : NetPacket
{
	static const uint8_t MAGIC = 0xFF;

	EventPacket(const uint8_t eventId)
		: magic_(0xFF)
		, eventId_(eventId)
                , size_(Size())
                , cksum_(0)
	{
		NetPacket::Add(magic_);
		NetPacket::Add(eventId_);
		NetPacket::Add(size_);
		NetPacket::Add(cksum_);
        }

	UInt8 magic_;
	UInt8 eventId_;
	UInt16 size_;
	UInt32 cksum_;
};

}
