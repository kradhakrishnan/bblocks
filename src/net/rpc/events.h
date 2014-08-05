#pragma once

#include "net/transport.h"
#include "net/rpc/serializable.h"
#include "net/rpc/rpc-data.h"

/**
 * This file contains implementation of basic message channel
 */
namespace dh_core {

struct EventPacket : Serializable
{
	EventPacket(const uint32_t eventId, const uint32_t size, const uint32_t cksum)
		: magic_(0xfefa)
		, eventId_(eventId)
		, size_(size)
		, cksum_(cksum)
	{
		Add(&magic_);
		Add(&eventId_);
		Add(&size_);
		Add(&cksum_);
	}

	UInt16 magic_;
	UInt16 eventId_;
	UInt32 size_;
	UInt32 cksum_;
};

class UnicastEventAcceptor
{
public:

	using This = UnicastEventAcceptor;

	UnicastEventAcceptor(const AutoPtr<UnicastAcceptor> & acceptor);
	UnicastEventAcceptor(UnicastAcceptor & acceptor);

	virtual int Accept(const SocketAddress & addr, Fn2<int, UnicastTransportChannel *> & cb)
	{
		server_.Accept(addr, async_fn(this, &This::AcceptDone));
		return 0;
	}

	void AcceptDone(int status, UnicastTransportChannel * ch);

private:

	AutoPtr<UnicastAcceptor> serverPtr_;
	UnicastAcceptor & server_;
};

class UnicastEventConnector
{
};

class UnicastEventChannel
{
};

}
