#include "net/rpc-transport.h"
#include "buf/bufpool.h"

using namespace std;
using namespace boost;

using namespace dh_core;
using namespace dh_core::rpc;

// ............................................................................... TCPTransport ....

void
TCPTransport::Read(TCPChannel & ch, IOBuffer & buf, ReadHandle & h)
{
	auto ctx = new (BufferPool::Alloc<ReadHandle>()) ReadHandle(h);
	bool status = ch.Read(buf, intr_fn(this, &This::ReadDone, ctx));
	if (status) {
		BufferPool::Dalloc<ReadHandle>(ctx);
		h.Wakeup(/*status=*/ 0, buf);
	}
}

void
TCPTransport::ReadDone(TCPChannel *, int status, IOBuffer buf, ReadHandle * h)
{
	AutoRelease<ReadHandle> _(h);
	h->Wakeup(status, buf);
}

void
TCPTransport::Write(TCPChannel & ch, IOBuffer & buf, WriteHandle & h)
{
	auto ctx = new (BufferPool::Alloc<WriteHandle>()) WriteHandle(h);
	int status = ch.Write(buf, intr_fn(this, &This::WriteDone, ctx));
	if ((unsigned int) status == buf.Size()) {
		BufferPool::Dalloc<WriteHandle>(ctx);
		h.Wakeup(/*status=*/ 0);
	}
}

void
TCPTransport::WriteDone(TCPChannel *, int status, WriteHandle * h)
{
	AutoRelease<WriteHandle> _(h);
	h->Wakeup(status);
}

// .......................................................................... TCPClientTransport ...

void
TCPClientTransport::Connect(const SocketAddress & addr, const StartHandle & h)
{
	tcpclient_.Connect(addr, intr_fn(this, &This::ConnectDone, new StartHandle(h)));
}

void
TCPClientTransport::ConnectDone(TCPConnector *, int status, TCPChannel * ch, StartHandle * h)
{
	if (status == 0) {
		ch_ = boost::shared_ptr<TCPChannel>(ch);
		h->Wakeup(true);
	} else {
		h->Wakeup(false);
	}

	delete h;
}

