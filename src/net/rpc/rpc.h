#pragma once

#include "async.h"
#include "net/rpc/rpc-data.h"
#include "net/transport.h"
#include "net/socket.h"

namespace bblocks {

enum
{
	RPC_SUCCESS = 0,
	RPC_FAIL = 1
};

enum
{
	RPC_HELLO = UINT8_MAX,
};

// ................................................................................ RpcEndpoint ....

struct RpcEndpoint
{
	RpcEndpoint() : name_(""), uuid_(UINT64_MAX) {}

	RpcEndpoint(const std::string & name, const uint64_t uuid)
		: name_(name), uuid_(uuid)
	{}

	std::string name_;
	uint64_t uuid_;
};

// ............................................................................... HelloMessage ....

struct HelloMessage : RPCData
{
	HelloMessage() {}

	HelloMessage(const RpcEndpoint & e)
		: name_(e.name_), uuid_(e.uuid_), pathid_(0), pathver_(0), cksum_(0)
	{}

	virtual void Encode(IOBuffer & buf, size_t & pos) override
	{
		INVARIANT(buf.Size() >= Size());

		name_.Encode(buf, pos);
		uuid_.Encode(buf, pos);
		pathid_.Encode(buf, pos);
		pathver_.Encode(buf, pos);
		cksum_.Encode(buf, pos);
	}

	virtual void Decode(IOBuffer & buf, size_t & pos) override
	{
		INVARIANT(buf.Size() >= Size());

		name_.Decode(buf, pos);
		uuid_.Decode(buf, pos);
		pathid_.Decode(buf, pos);
		pathver_.Decode(buf, pos);
		cksum_.Decode(buf, pos);
	}

	virtual size_t Size() const override
	{
		return name_.Size() + uuid_.Size() + pathid_.Size() + pathver_.Size()
		       + cksum_.Size();
	}

	String name_;
	UInt64 uuid_;
	UInt16 pathid_;
	UInt16 pathver_;
	UInt32 cksum_;
};

// ........................................................................... RpcRequestHeader ....

/*
 * Rpc Packet Layout
 * =================
 *
 * +----------------------------------------------------+ -+
 * | opcode (8) | size (16)    | cksum (32)             |  |
 * +----------------------------------------------------+  +--> RpcRequestHeader
 * | Seq number	(16)  |	 	 	                |  |
 * +----------------------------------------------------+ -+
 * | Packet data                                        |
 * .                                                    .
 * .                                                    .
 * |                                                    |
 * +----------------------------------------------------+
 *
 */
struct RpcRequestHeader : RPCData
{
	RpcRequestHeader() {}

	RpcRequestHeader(uint16_t seqno, uint8_t opcode, uint16_t size, uint32_t cksum)
		: seqno_(seqno), opcode_(opcode), size_(size), cksum_(cksum)
	{}

	virtual void Encode(IOBuffer & buf, size_t & pos) override
	{
		INVARIANT(buf.Size() >= Size());

		seqno_.Encode(buf, pos);
		opcode_.Encode(buf, pos);
		size_.Encode(buf, pos);
		cksum_.Encode(buf, pos);
	}

	virtual void Decode(IOBuffer & buf, size_t & pos) override
	{
		INVARIANT(buf.Size() >= Size());

		seqno_.Decode(buf, pos);
		opcode_.Decode(buf, pos);
		size_.Decode(buf, pos);
		cksum_.Decode(buf, pos);
	}

	virtual size_t Size() const
	{
		return seqno_.Size() + opcode_.Size() + size_.Size() + cksum_.Size();
	}

	UInt16 seqno_;
	UInt8 opcode_;
	UInt16 size_;
	UInt32 cksum_;
};

// ................................................................................ RpcResponse ....

struct RpcResponseHeader : public RPCData
{
	RpcResponseHeader() {}

	RpcResponseHeader(uint16_t seqno, uint8_t opcode, uint16_t size, uint32_t cksum,
			  uint8_t status)
		: seqno_(seqno), opcode_(opcode), size_(size), cksum_(cksum), status_(status)
	{}

	virtual void Encode(IOBuffer & buf, size_t & pos) override
	{
		INVARIANT(buf.Size() >= Size());

		seqno_.Encode(buf, pos);
		opcode_.Encode(buf, pos);
		size_.Encode(buf, pos);
		cksum_.Encode(buf, pos);
		status_.Encode(buf, pos);
	}

	virtual void Decode(IOBuffer & buf, size_t & pos) override
	{
		INVARIANT(buf.Size() >= Size());

		seqno_.Decode(buf, pos);
		opcode_.Decode(buf, pos);
		size_.Decode(buf, pos);
		cksum_.Decode(buf, pos);
		status_.Decode(buf, pos);
	}

	virtual size_t Size() const
	{
		return seqno_.Size() + opcode_.Size() + size_.Size() + cksum_.Size()
		       + status_.Size();
	}

	UInt16 seqno_;
	UInt8 opcode_;
	UInt16 size_;
	UInt32 cksum_;
	UInt8 status_;
};

// .................................................................................. RpcPacket ....

struct RpcRequest
{
	RpcRequest(const RpcEndpoint & endpoint, const RpcRequestHeader & hdr,
		   const IOBuffer & data)
		: endpoint_(endpoint), hdr_(hdr), data_(data)
	{}

	RpcEndpoint endpoint_;
	RpcRequestHeader hdr_;
	IOBuffer data_;

};

// ................................................................................ RpcResponse ....

struct RpcResponse
{
	RpcResponse(const RpcEndpoint & endpoint, const RpcResponseHeader & hdr,
		    const IOBuffer & buf)
	{}

	RpcEndpoint endpoint_;
	RpcResponseHeader hdr_;
	IOBuffer data_;
};

// .................................................................................. RpcServer ....

class RpcServer
{
public:

	using This = RpcServer;

	using StopDoneHandle = Fn<int>;
	using ErrorHandle = Fn<int>;

	RpcServer(UnicastAcceptor & acceptor);
	~RpcServer();

	int Start(const SocketAddress & addr, const ErrorHandle & h);
	int Stop(const StopDoneHandle & h);

	int Register(int id, const Fn<RpcRequest> & h); 

private:

	struct RpcChannel
	{
		RpcChannel(UnicastTransportChannel * ch = NULL) : isActive_(false), ch_(ch) {}

		bool isActive_;
		UnicastTransportChannel * ch_;
		RpcRequestHeader hdr_;
		RpcEndpoint endpoint_;
	};

	/*
	 *  Data flow
	 *  =========
         *
	 * +-> Start *--> AcceptDone --> (X) ReadHeader *--> ReadHeaderDone *--> ReadDataDone
	 *      {if first tranmission} *--> WriteHelloReponseDone --> (X)
	 *	{else} --> DispatchRpc --> (X)
	 *
	 */ 

	void ReadHeader(RpcChannel * ch);
	void HandleHello(RpcChannel * ch, IOBuffer & buf);

	int DispatchRpcCall(const RpcRequest & packet);

	void AcceptDone(int status, UnicastTransportChannel * ch) __async_fn__;
	void StopDone(int status) __async_fn__;
	void ReadHeaderDone(int status, IOBuffer buf, RpcChannel * ch) __async_fn__;
	void ReadDataDone(int status, IOBuffer bug, RpcChannel * ch) __async_fn__;
	void WriteHelloResponseDone(int status, IOBuffer buf, RpcChannel * ch) __async_fn__;

	using rpc_map_t = std::map<int, Fn<RpcRequest> >;

	const std::string fqn_;
	SpinMutex lock_;
	UnicastAcceptor & acceptor_;
	rpc_map_t rpcs_;
	std::atomic<size_t> pendingStart_;
	std::atomic<size_t> pendingStop_;
	StopDoneHandle stoph_;
	ErrorHandle errh_;
};

// .................................................................................. RpcClient ....

class RpcClient
{
public:

    using This = RpcClient;

    using ConnDoneHandle = Fn<int>;

    RpcClient(const RpcEndpoint & endpoint, UnicastConnector & connector);
    ~RpcClient();

    int Connect(const SocketAddress & addr, const ConnDoneHandle & h);

private:

    void ConnDone(int status, UnicastTransportChannel * ch) __async_fn__;
    void HelloWriteDone(int status, IOBuffer buf, UnicastTransportChannel * ch) __async_fn__;

    const std::string fqn_;
    const RpcEndpoint endpoint_;
    UnicastConnector & connector_;
    std::atomic<size_t> pendingConns_;
    ConnDoneHandle connh_;
};

}
