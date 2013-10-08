#pragma once

#include <memory>
#include <string>

#include "async.h"
#include "net/tcp-linux.h"
#include "net/rpc-data.h"

namespace dh_core { namespace rpc {

using namespace dh_core;

// .................................................................................. Transport ....

class Transport
{
public:

	typedef Fn<bool> StopDoneHandle;
	typedef Fn2<int, IOBuffer> ReadDoneHandle;
	typedef Fn<int> WriteDoneHandle;

	virtual bool Stop(const StopDoneHandle & cb) = 0;

	virtual bool Peek(IOBuffer & data, const ReadDoneHandle & h) = 0;
	virtual bool Read(IOBuffer & buf, const ReadDoneHandle & h) = 0;
	virtual bool Write(IOBuffer & buf, const WriteDoneHandle & h) = 0;
};

// ............................................................................... TCPTransport ....

class TCPTransportChannel : public CHandle, public Transport, public AsyncProcessor
{
public:

	friend class TCPServerTransport;
	friend class TCPClientTransport;

	typedef TCPTransportChannel This;

	virtual ~TCPTransportChannel()
	{
		INVARIANT(!pendingios_);
	}

	virtual bool Stop(const StopDoneHandle & cb) override;
	virtual bool Peek(IOBuffer & data, const ReadDoneHandle & h) override;
	virtual bool Read(IOBuffer & buf, const ReadDoneHandle & h) override;
	virtual bool Write(IOBuffer & buf, const WriteDoneHandle & h) override;

	__STATELESS_ASYNC_PROCESSOR__

protected:

	TCPTransportChannel(const std::shared_ptr<TCPChannel> & ch)
		: pendingios_(0), ch_(ch)
	{}

	void ReadDone(TCPChannel *, int status, IOBuffer buf, ReadDoneHandle * h) __intr_fn__;
	void WriteDone(TCPChannel *, int status, WriteDoneHandle * h) __intr_fn__;
	void PeekDone(TCPChannel *, int status, IOBuffer buf, ReadDoneHandle * h) __intr_fn__;

	std::atomic<size_t> pendingios_;
	std::shared_ptr<TCPChannel> ch_;
};

// ............................................................................ TransportServer ....

class ServerTransport
{
public:

	typedef Fn<bool> StopDoneHandle;
	typedef Fn2<bool, Transport *> ListenDoneHandle;

	virtual void Listen(const SocketAddress & addr, const ListenDoneHandle & h) = 0;
	virtual void Stop(const StopDoneHandle & h) = 0;
};

// ......................................................................... TCPServerTransport ....

class TCPServerTransport : public ServerTransport
{
public:

	typedef TCPServerTransport This;

	TCPServerTransport(const std::string path = "/rpc/transport/tcpserver")
	    : log_(path), lock_(path), epoll_(path)
	    , tcpserver_(epoll_)
	{}

	virtual void Listen(const SocketAddress & addr, const ListenDoneHandle & h) override;
	virtual void Stop(const StopDoneHandle & h) override;

private:

	void ListenDone(TCPServer * s, int status, TCPChannel * ch,
		        ListenDoneHandle * h) __intr_fn__;

	const LogPath log_;

	SpinMutex lock_;
	std::vector<SocketAddress> addr_;
	Epoll epoll_;
	TCPServer tcpserver_;
};

// ............................................................................ TransportClient ....

class ClientTransport
{
public:

	typedef Fn2<bool, Transport *> ConnectDoneHandle;

	virtual void Connect(const SocketAddress & addr, const ConnectDoneHandle & h) = 0;
};

// ......................................................................... TCPClientTransport ....

class TCPClientTransport : public ClientTransport
{
public:

	typedef TCPClientTransport This;

	TCPClientTransport(Epoll & epoll)
	    : log_("/rpc/transport/tcpclient")
	    , epoll_(epoll)
	    , tcpclient_(epoll)
	{}

	virtual ~TCPClientTransport() {}

	virtual void Connect(const SocketAddress & addr, const ConnectDoneHandle & h) override;

private:

	void ConnectDone(TCPConnector *, int status, TCPChannel * ch,
			 ConnectDoneHandle * h) __intr_fn__;

	const LogPath log_;

	Epoll & epoll_;
	TCPConnector tcpclient_;
};


} } // namespace dh_core::rpc
