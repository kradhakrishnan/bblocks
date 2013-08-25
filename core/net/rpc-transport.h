#ifndef _CORE_NET_RPC_TRANSPORT_H_
#define _CORE_NET_RPC_TRANSPORT_H_

#include <string>

#include "core/async.h"
#include "core/net/tcp-linux.h"
#include "core/net/rpc-data.h"

namespace dh_core { namespace rpc {

using namespace dh_core;

// ............................................................... Endpoint ....

class Endpoint
{
public:

	Endpoint(const std::string & name, const uint64_t uuid)
		: name_(name), uuid_(uuid)
	{}

	const std::string & GetName() const { return name_; }
	uint64_t GetUID() const { return uuid_; }

protected:

	const std::string name_;
	const uint64_t uuid_;

};

// .............................................................. Transport ....

class Transport
{
public:

	typedef Fn2<bool, uint64_t> StartCallback;
	typedef Fn<bool> StopCallback;
	typedef Fn2<int, uint8_t *> PeekDoneFn;
	typedef Fn2<int, IOBuffer> ReadDoneFn;
	typedef Fn<int> WriteDoneFn;

	virtual void NotifyError(const Fn<int> & cb);
	virtual void NotifyEndpointDiscovery(const Fn<Endpoint> & cb);
	virtual void NotifyEnpointClosure(const Fn<Endpoint> & cb);

	virtual bool Start(const Fn<bool> & cb) = 0;
	virtual bool Stop(const Fn<bool> & cb) = 0;
	virtual bool Peek(const uint64_t euid, const size_t size,
			  uint8_t * data, const PeekDoneFn & fn) = 0;
	virtual bool Read(const uint64_t euid, IOBuffer & buf,
			  const ReadDoneFn & fn) = 0;
	virtual bool Write(const uint64_t euid, IOBuffer & buf,
			   const WriteDoneFn & fn) = 0;
};

// ..................................................... TCPServerTransport ....

class TCPServerTransport : public CompletionHandle, public Transport,
			   public AsyncProcessor
{
public:

	TCPServerTransport(const Endpoint & endpoint,
			   const SocketAddress & addr);
	TCPServerTransport(const Endpoint & endpoint,
			   const std::vector<SocketAddress> & addrs);

	virtual bool Start(const StartCallback & cb);
	virtual bool Stop(const StopCallback & cb);
	virtual bool Peek(const Endpoint & endpoint, const size_t size,
			  uint8_t * data, const PeekDoneFn & cb);
	virtual bool Read(Endpoint & endpoint, IOBuffer & buf,
			  const ReadDoneFn & cb);
	virtual bool Write(Endpoint & endpoint, IOBuffer & buf,
			   const WriteDoneFn & cb);

private:
};

// ..................................................... TCPClientTransport ....

class TCPClientTransport : public CompletionHandle, public Transport,
			   public AsyncProcessor
{
public:

	TCPClientTransport(const Endpoint & endpoint);

	void AddPath(const Endpoint & endpoint, const SocketAddress & addr);
	void AddPath(const Endpoint & endpoint,
		     const std::vector<SocketAddress> & addrs);

	virtual bool Start(const StartCallback & cb);
	virtual bool Stop(const StopCallback & cb);
	virtual bool Peek(const uint64_t uuid, const size_t size,
			  uint8_t * data, const PeekDoneFn & cb);
	virtual bool Read(const uint64_t uid, IOBuffer & buf,
			  const ReadDoneFn & cb);
	virtual bool Write(const uint64_t endpoint, IOBuffer & buf,
			   const WriteDoneFn & cb);

private:
};

// ........................................................... RPCSynPacket ....

struct RPCSynPacket : RPCPacket
{
	UInt64 magic_;
	UInt64 uid_;
	String name_;
	UInt64 serveruid_;
	UInt64 path_;
	UInt64 pathVersion_;
};

} } // namespace dh_core::rpc

#endif
