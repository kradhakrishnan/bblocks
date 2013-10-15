#include "net/rpc-transport.h"
#include "buf/bufpool.h"

using namespace std;

using namespace dh_core;
using namespace dh_core::rpc;

// ............................................................................... TCPTransport ....

bool
TCPTransportChannel::Peek(IOBuffer & buf, const ReadDoneHandle & h)
{
	++pendingios_;

	auto ctx = new (BufferPool::Alloc<ReadDoneHandle>()) ReadDoneHandle(h);
	bool status = ch_->Peek(buf, intr_fn(this, &This::PeekDone, ctx));

	if (status) {
		PeekDone(ch_.get(), status, buf, ctx);
	}

	return status;
}

void
TCPTransportChannel::PeekDone(TCPChannel * ch, int status, IOBuffer buf, ReadDoneHandle * h)
{
	ASSERT(ch_.get() == ch);

	--pendingios_;

	AutoRelease<ReadDoneHandle> _(h);
	h->Wakeup(status, buf);
}

bool
TCPTransportChannel::Read(IOBuffer & buf, const ReadDoneHandle & h)
{
	++pendingios_;

	auto ctx = new (BufferPool::Alloc<ReadDoneHandle>()) ReadDoneHandle(h);
	bool status = ch_->Read(buf, intr_fn(this, &This::ReadDone, ctx));

	if (status) {
		ReadDone(ch_.get(), buf.Size(), buf, ctx);
	}

	return status;
}

void
TCPTransportChannel::ReadDone(TCPChannel * ch, int status, IOBuffer buf, ReadDoneHandle * h)
{
	INVARIANT(status == (int) buf.Size());
	ASSERT(buf);
	ASSERT(ch_.get() == ch);

	--pendingios_;

	AutoRelease<ReadDoneHandle> _(h);
	h->Wakeup(status, buf);
}

bool
TCPTransportChannel::Write(IOBuffer & buf, const WriteDoneHandle & h)
{
	++pendingios_;

	auto ctx = new (BufferPool::Alloc<WriteDoneHandle>()) WriteDoneHandle(h);
	int status = ch_->Write(buf, intr_fn(this, &This::WriteDone, ctx));

	if ((unsigned int) status == buf.Size()) {
		WriteDone(ch_.get(), status, ctx);
	}

	return status;
}

void
TCPTransportChannel::WriteDone(TCPChannel * ch, int status, WriteDoneHandle * h)
{
	ASSERT(ch_.get() == ch);

	--pendingios_;

	AutoRelease<WriteDoneHandle> _(h);
	h->Wakeup(status);
}

bool
TCPTransportChannel::Stop(const StopDoneHandle & h)
{
	stoph_ = h;
	ch_->UnregisterHandle((CHandle *) this, (UnregisterDoneFn) &This::Unregistered);
	return false;
}

void
TCPTransportChannel::Unregistered(int status)
{
	INVARIANT(status == 0);

	ch_->Close();
	stoph_.Wakeup(/*status=*/ true);
}

// .......................................................................... TCPClientTransport ...

void
TCPClientTransport::Connect(const SocketAddress & addr, const ConnectDoneHandle & h)
{
	tcpclient_.Connect(addr, intr_fn(this, &This::ConnectDone, new ConnectDoneHandle(h)));
}

void
TCPClientTransport::ConnectDone(TCPConnector *, int status, TCPChannel * ch, ConnectDoneHandle * h)
{
	if (status == 0) {
		auto tch = shared_ptr<TCPChannel>(ch);
		h->Wakeup(true,  new TCPTransportChannel(tch));
	} else {
		ERROR(log_) << "Error connecting to server. status=" << status;

		h->Wakeup(false, /*transport=*/ NULL);
	}

	delete h;
}

// ......................................................................... TCPServerTransport ....

void
TCPServerTransport::Listen(const SocketAddress & addr, const ListenDoneHandle & h)
{
	listenDoneHandle_ = h;
	tcpserver_.Listen(addr.LocalAddr(), async_fn(this, &This::ListenDone));
}

void
TCPServerTransport::ListenDone(TCPServer * s, int status, TCPChannel * ch)
{
	if (status != 0) {
		/*
		 * Failed to accept incoming connection
		 */
		listenDoneHandle_.Wakeup(/*status=*/ false, /*transport=*/ NULL);
		return;
	}

	auto tch = shared_ptr<TCPChannel>(ch);
	listenDoneHandle_.Wakeup(/*status=*/ true, new TCPTransportChannel(tch)); 
}

void
TCPServerTransport::Stop(const StopDoneHandle & h)
{
	Guard _(&lock_);

	tcpserver_.Shutdown();
	((StopDoneHandle &) h).Wakeup(/*status=*/ true);
}
