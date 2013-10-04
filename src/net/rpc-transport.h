#ifndef _CORE_NET_RPC_TRANSPORT_H_
#define _CORE_NET_RPC_TRANSPORT_H_

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

	typedef Fn<bool> StopHandle;
	typedef Fn2<int, IOBuffer> ReadHandle;
	typedef Fn<int> WriteHandle;

	virtual bool Stop(const StopHandle & cb) = 0;

	// virtual bool Peek(IOBuffer & data, const ReadHandle & h) = 0;
	virtual bool Read(IOBuffer & buf, const ReadHandle & h) = 0;
	virtual bool Write(IOBuffer & buf, const WriteHandle & h) = 0;
};

// ............................................................................... TCPTransport ....

class TCPTransport : public CHandle, public Transport, public AsyncProcessor
{
public:

	virtual ~TCPTransport() {}

protected:

	typedef TCPTransport This;

	// void Peek(TCPChannel & ch, IOBuffer & buf, const ReadHandle & h);
	void Read(TCPChannel & ch, IOBuffer & buf, ReadHandle & h);
	void Write(TCPChannel & ch, IOBuffer & buf, WriteHandle & h);

	__interrupt__ void ReadDone(TCPChannel *, int status, IOBuffer buf, ReadHandle * h);
	__interrupt__ void WriteDone(TCPChannel *, int status, WriteHandle * h);
};

// ......................................................................... TCPServerTransport ....

class TCPServerTransport : public TCPTransport
{
public:

	typedef Fn<bool> StartHandle;

	TCPServerTransport(const std::string path = "/rpc/transport/tcpserver")
	    : log_(path), lock_(path), epoll_(path)
	    , tcpserver_(epoll_)
	{}

	virtual bool Listen(const SocketAddress & addr, const StartHandle & h);
	virtual bool Listen(const std::vector<SocketAddress> & addr, StartHandle & h);

	virtual bool Stop(const StopHandle & h) override;

	// virtual bool Peek(IOBuffer & buf, const ReadHandle & h) override;
	virtual bool Read(IOBuffer & buf, const ReadHandle & h) override;
	virtual bool Write(IOBuffer & buf, const WriteHandle & h) override;

private:

	const LogPath log_;

	SpinMutex lock_;
	std::vector<SocketAddress> addr_;
	Epoll epoll_;
	TCPServer tcpserver_;
	std::list<boost::shared_ptr<TCPChannel> > chs_;
};

// ......................................................................... TCPClientTransport ....

class TCPClientTransport : public TCPTransport
{
public:

	typedef TCPClientTransport This;
	typedef Fn<bool> StartHandle;

	TCPClientTransport(Epoll & epoll)
	    : log_("/rpc/transport/tcpclient")
	    , lock_("/rpc/transport/tcpclient")
	    , epoll_(epoll)
	    , tcpclient_(epoll)
	{}

	virtual ~TCPClientTransport() {}

	void Connect(const SocketAddress & addr, const StartHandle & cb);
	virtual bool Stop(const StopHandle & cb) override;

	// virtual bool Peek(IOBuffer & buf, const PeekHandle & h) override;
	virtual bool Read(IOBuffer & buf, const ReadHandle & h) override;
	virtual bool Write(IOBuffer & buf, const WriteHandle & h) override;

private:

	__interrupt__ void ConnectDone(TCPConnector *, int status, TCPChannel * ch,
				       StartHandle * h);

	const LogPath log_;

	SpinMutex lock_;
	Epoll & epoll_;
	TCPConnector tcpclient_;
	boost::shared_ptr<TCPChannel> ch_;
};


} } // namespace dh_core::rpc

#endif
