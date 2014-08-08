#include <memory>

#include "net/rpc/rpc.h"

using namespace bblocks;
using namespace std;


// .................................................................................. RpcServer ....

RpcServer::RpcServer(UnicastAcceptor & acceptor)
    : fqn_("/RpcServer/" + STR(this))
    , lock_(fqn_)
    , acceptor_(acceptor)
    , pendingStart_(0)
    , pendingStop_(0)
{
}

RpcServer::~RpcServer()
{
	INVARIANT(!pendingStart_);
	INVARIANT(!pendingStop_);
}

int
RpcServer::Register(int id, const Fn<RpcRequest> & h)
{
	Guard _(&lock_);

	INVARIANT(rpcs_.find(id) == rpcs_.end());

	int status = rpcs_.insert(make_pair(id, h)).second;
	INVARIANT(status);

	return 0;
}

int
RpcServer::Start(const SocketAddress & addr, const ErrorHandle & h)
{
	LOG_INFO << "Stating RPC server";

	errh_ = h;

	++pendingStart_;
	return acceptor_.Accept(addr, async_fn(this, &This::AcceptDone));
}

void
RpcServer::AcceptDone(int status, UnicastTransportChannel * ch)
{
	LOG_DEBUG << "Accepted. status=" << status;

	if (status != 0) {
		/*
		 * Error accepting channel, notify the caller.
		 */
		LOG_ERROR << "Error accepting connection. status=" << status;

		 errh_.Wakeup(/*status=*/ status);
		 return;
	}

	INVARIANT(ch);

	RpcChannel * rpcch = new RpcChannel(ch);

	ReadHeader(rpcch);
}

void
RpcServer::ReadHeader(RpcChannel * ch)
{
	IOBuffer buf = IOBuffer::Alloc(RpcRequestHeader().Size());

	int err = ch->ch_->Read(buf, async_fn(this, &This::ReadHeaderDone, ch));

	if (err == -1) {
		errh_.Wakeup(/*status=*/ err);
	} else if (err == (int) buf.Size()) {
		ReadHeaderDone(err, buf, ch);
	}
}

void
RpcServer::ReadHeaderDone(int status, IOBuffer buf, RpcChannel * ch)
{
	LOG_DEBUG << "Read header. ch=" << ch->ch_ << " status=" << status;
 
	if (status != (int) buf.Size()) {
		DEADEND
	}

	LOG_DEBUG << buf.Dump();

	size_t pos = 0;
	ch->hdr_.Decode(buf, pos);

	LOG_DEBUG << "Rpc packet decoded. "
		  << " seqno=" << ch->hdr_.seqno_.Get()
		  << " opcode=" << (int) ch->hdr_.opcode_.Get()
		  << " size=" << ch->hdr_.size_.Get()
		  << " cksum=" << ch->hdr_.cksum_.Get();

	if (!ch->isActive_ && ch->hdr_.opcode_.Get() != RPC_HELLO) {
		/*
		 * The Firs transmission has to be RPC_HELLO
		 */
		errh_.Wakeup(/*status=*/ -1);
		return;
	}

	IOBuffer data = IOBuffer::Alloc(ch->hdr_.size_.Get());

	int err = ch->ch_->Read(data, async_fn(this, &This::ReadDataDone, ch));

	if (err == -1) {
		errh_.Wakeup(err);
	} else if (err == (int) data.Size()) {
		ReadDataDone(err, data, ch);
	} else {
		INVARIANT(err >= 0 && err < (int) data.Size());
	}
}

void
RpcServer::HandleHello(RpcChannel * ch, IOBuffer & buf)
{
	HelloMessage msg;
	size_t pos = 0;
	msg.Decode(buf, pos);

	ch->endpoint_ = RpcEndpoint(msg.name_.Get(), msg.uuid_.Get());
	ch->isActive_ = true;

	LOG_INFO << "Channel endpoint discovered. name=" << ch->endpoint_.name_
		 << " uuid=" << ch->endpoint_.uuid_;

	RpcResponseHeader hdr(/*seqno=*/ 0, /*opcode=*/ RPC_HELLO, /*size=*/ 0,
			      /*cksum=*/ 0, /*status=*/ 0);

	IOBuffer data = IOBuffer::Alloc(hdr.Size());
	pos = 0;
	hdr.Encode(buf, pos);

	hdr.cksum_ = Adler32::Calc(data.Ptr(), data.Size());
	pos = 0;
	hdr.Encode(buf, pos);

	int err = ch->ch_->Write(data, async_fn(this, &This::WriteHelloResponseDone, ch));

	if (err == -1) {
	} else if (err == (int) data.Size()) {
		WriteHelloResponseDone(err, data, ch);
	} else {
		INVARIANT(err > 0 && err < (int) data.Size());
	}
}

void
RpcServer::WriteHelloResponseDone(int status, IOBuffer buf, RpcChannel * ch)
{
	if (status != (int) buf.Size()) {
		DEADEND
	}

	ReadHeader(ch);
}

void
RpcServer::ReadDataDone(int status, IOBuffer buf, RpcChannel * ch)
{
	if (status != (int) buf.Size()) {
		DEADEND
	}

	/*
	 * Validate packet checksum
	 */
	uint32_t expected = ch->hdr_.cksum_.Get(); 
	uint32_t cksum = Adler32::Calc(buf.Ptr(), buf.Size());
	if (cksum != expected) {
		LOG_ERROR << "Checksum did not match. ch=" << ch
			  << " expected=" << cksum << " actual=" << cksum;
		errh_.Wakeup(/*status=*/ -1);
		return;
	}

	/*
	 * Handle if it is HELLO packet explicitly
	 */
	if (!ch->isActive_) {
		HandleHello(ch, buf);
		return;
	}

	/*
	 * Dispatch RPC call to the handler
	 */
	RpcRequest packet(ch->endpoint_, ch->hdr_, buf);
	int err = DispatchRpcCall(packet);

	if (err == -1) {
		DEADEND
	}

	ReadHeader(ch);
}

int
RpcServer::DispatchRpcCall(const RpcRequest & packet)
{
	auto it = rpcs_.find(packet.hdr_.opcode_.Get());

	ASSERT(it != rpcs_.end());
	if (it == rpcs_.end()) {
		LOG_ERROR << "Command not found. command=" << packet.hdr_.opcode_.Get();
		return -1;
	}

	it->second.Wakeup(packet);
	return 0;
}

int RpcServer::Stop(const StopDoneHandle & h)
{
	DEADEND
	return 0;
}

void
RpcServer::StopDone(int status)
{
	DEADEND
}

// .................................................................................. RcpClient ....

RpcClient::RpcClient(const RpcEndpoint & endpoint, UnicastConnector & connector)
	: fqn_("/rpcclient"), endpoint_(endpoint), connector_(connector), pendingConns_(0)
{
}

RpcClient::~RpcClient()
{
	INVARIANT(!pendingConns_);
}

int
RpcClient::Connect(const SocketAddress & addr, const ConnDoneHandle & h)
{
	connh_ = h;

	++pendingConns_;

	return connector_.Connect(addr, async_fn(this, &This::ConnDone));
}

void
RpcClient::ConnDone(int status, UnicastTransportChannel * ch)
{
	LOG_INFO << "ConnDone. status=" << status << " ch=" << ch;

	if (status != 0) {
		connh_.Wakeup(/*status=*/ 0);
		return;
	}

	INVARIANT(ch);

	const size_t hdrsize = RpcRequestHeader().Size();

	HelloMessage msg(endpoint_);

	IOBuffer buf = IOBuffer::Alloc(hdrsize + msg.Size());

	size_t pos = hdrsize;
	msg.Encode(buf, pos);

	uint32_t cksum = Adler32::Calc(buf.Ptr() + hdrsize, msg.Size());
	RpcRequestHeader hdr(/*seqno=*/ 0, /*opcode=*/ RPC_HELLO, msg.Size(), cksum);
	pos = 0;
	hdr.Encode(buf, pos);

	LOG_DEBUG << "Sending hello. ch=" << ch << " bytes=" << buf.Size()
		  << " buf=" << buf.Dump();

	int err = ch->Write(buf, async_fn(this, &This::HelloWriteDone, ch));

	if (err == -1) {
		connh_.Wakeup(/*status=*/ 0);
	} else if (err == (int) buf.Size()) {
		HelloWriteDone(err, buf, ch);
	}
}

void
RpcClient::HelloWriteDone(int status, IOBuffer buf, UnicastTransportChannel * ch)
{
}
