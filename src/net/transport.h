#pragma once

#include <memory>
#include <string>

#include "async.h"
#include "net/socket.h"
#include "buf/buffer.h"

namespace dh_core {

// .................................................................................. Transport ....

class UnicastTransportChannel : public AsyncProcessor
{
public:

	virtual ~UnicastTransportChannel() {}

	typedef Fn<int> StopDoneHandle;
	typedef Fn2<int, IOBuffer> ReadDoneHandle;
	typedef Fn2<int, IOBuffer> WriteDoneHandle;

	/**
	 * Invoke asynchronous peek operation on the byte stream
	 *
	 * @param   data    Buffer to copy the data into
	 * @param   h	    Completion handle
	 * @return  -1 on error or bytes peeked on success
	 *
	 * Note : If the bytes returned equals the size of buffer, there will 
	 * be no asynchronous callback on the completion handle
	 */
	virtual int Peek(IOBuffer & data, const ReadDoneHandle & h) = 0;

	/**
	 * Invoke asynchronous read operation on the byte stream
	 *
	 * @param   data    Buffer to copy the data into
	 * @param   h	    Completion handle
	 * @return  -1 on error or bytes peeked on success
	 *
	 * Note : If the bytes returned equals the size of buffer, there will 
	 * be no asynchronous callback on the completion handle
	 */
	virtual int Read(IOBuffer & buf, const ReadDoneHandle & h) = 0;

	/**
	 * Invoke asynchronous write operation to the byte stream
	 *
	 * @param   data    Buffer to copy the data into
	 * @param   h	    Completion handle
	 * @return  -1 on error or bytes peeked on success
	 *
	 * Note : If the bytes returned equals the size of buffer, there will 
	 * be no asynchronous callback on the completion handle
	 */
	virtual int Write(IOBuffer & buf, const WriteDoneHandle & h) = 0;

	/**
	 * Stop transport
	 *
	 * @parma   h	Completion handle
	 * @return -1 on error and 0 on success
	 */
	virtual int Stop(const StopDoneHandle & cb) = 0;
};

// ............................................................................ ServerTransport ....

class UnicastAcceptor : public AsyncProcessor
{
public:

	typedef Fn<int> StopDoneHandle;
	typedef Fn2<int, UnicastTransportChannel *> AcceptDoneHandle;

	virtual ~UnicastAcceptor() {}

	/**
	 * Start accepting connections on the specified address
	 *
	 * @param   addr    Address to listen on
	 * @param   h	    Completion handle
	 * @return  -1 on error or 0 to indicate successful start of async operation
	 */
	virtual int Accept(const SocketAddress & addr, const AcceptDoneHandle & h) = 0;

	/**
	 * Stop accepting new connection
	 *
	 * @param   h	Completion handle
	 * @return  -1 on error or 0 on successful start of async operation
	 */
	virtual int Stop(const StopDoneHandle & h) = 0;

private:

	__STATELESS_ASYNC_PROCESSOR__
};

// ............................................................................ ClientTransport ....

class UnicastConnector : public AsyncProcessor
{
public:

	typedef Fn<int> StopDoneHandle;
	typedef Fn2<int, UnicastTransportChannel *> ConnectDoneHandle;

	virtual ~UnicastConnector() {}

	/**
	 * Start establishing connection to the server
	 *
	 * @param   addr    Address to connect to
	 * @param   h	    Completion handle
	 * @return  -1 on error or 0 on successful start of async operation
	 */
	virtual int Connect(const SocketAddress & addr, const ConnectDoneHandle & h) = 0;

	/**
	 * Stop IO on this channel
	 *
	 * @param   h	Completion handle
	 */
	virtual int Stop(const StopDoneHandle & h) = 0;

private:

	__STATELESS_ASYNC_PROCESSOR__
};

} // namespace dh_core::rpc
