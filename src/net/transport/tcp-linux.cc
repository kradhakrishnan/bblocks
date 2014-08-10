#include "net/transport/tcp-linux.h"


using namespace std;
using namespace bblocks;

//.................................................................................. TCPChannel ....

TCPChannel::TCPChannel(const std::string & fqn, int fd, FdPoll & epoll)
	: fqn_(fqn)
	, lock_(fqn_)
	, fd_(fd)
	, epoll_(epoll)
	/* Perf Counters */
	, statReadSize_("stat/read-io", "bytes", PerfCounter::BYTES)
	, statWriteSize_("stat/write-io", "bytes", PerfCounter::BYTES)
{
	ASSERT(fd_ >= 0);

	const bool ok= epoll_.Add(fd_, EPOLLIN | EPOLLOUT | EPOLLET,
				  intr_fn(this, &TCPChannel::HandleFdEvent));
	INVARIANT(ok);
}

TCPChannel::~TCPChannel()
{
    LOG_VERBOSE << statReadSize_;
    LOG_VERBOSE << statWriteSize_;
}

int
TCPChannel::Write(IOBuffer & buf, const WriteDoneHandle & h)
{
	ASSERT(buf);

	Guard _(&lock_);

	if (wpending_.empty()) {
		/*
		 * There is no backlog, trying writing synchronously
		 */
		 wpending_.push_back(WriteCtx(buf, h));
		 return WriteDataToSocket(/*isasync=*/ false);
	}

	wpending_.push_back(WriteCtx(buf, h));

	/*
	 * Kick fd handler to write data out
	 */
	HandleFdEvent(fd_, EPOLLOUT);

	return 0;
}

int
TCPChannel::Read(IOBuffer & data, const ReadDoneHandle & h)
{
	return Read(data, h, /*peek=*/ false);
}

int
TCPChannel::Peek(IOBuffer & data, const ReadDoneHandle & h)
{
	return Read(data, h, /*peek=*/ true);
}

int
TCPChannel::Read(const IOBuffer & data, const ReadDoneHandle & h, const bool peek)
{
	ASSERT(data);

	Guard _(&lock_);

	INVARIANT(!rpending_.buf_ && !rpending_.bytesRead_);
    
	rpending_ = ReadCtx(data, h, peek);

	return ReadDataFromSocket(/*isasync=*/ false);
}

int
TCPChannel::Stop(const StopDoneHandle & h)
{
	ASSERT(h)

	const bool status = epoll_.Remove(fd_);
	INVARIANT(status);

	INVARIANT(!stoph_);
	stoph_ = h;

	ThreadPool::ScheduleBarrier(this, &TCPChannel::BarrierDone, /*nonce=*/ 0);

	return 0;
}

void
TCPChannel::BarrierDone(int)
{
	{
		Guard _(&lock_);

		Close();
		FailOps();

		INVARIANT(wpending_.empty());
		INVARIANT(!rpending_.h_);
	}

	stoph_.Wakeup(/*status=*/ 0);
	stoph_ = NULL;
}

void
TCPChannel::Close()
{
	LOG_DEBUG << "Closing channel " << fd_;

	::shutdown(fd_, SHUT_RDWR);
	::close(fd_);
}

void
TCPChannel::HandleFdEvent(int fd, uint32_t events)
{
	ASSERT(fd == fd_);
	ASSERT(!(events & ~(EPOLLIN | EPOLLOUT | EPOLLHUP | EPOLLERR)));

	LOG_DEBUG << "Epoll Notification: fd=" << fd_ << " events:" << events;

	Guard _(&lock_);

	if (events & EPOLLIN) {
		ReadDataFromSocket(/*isasync=*/ true);
	}

	if (events & EPOLLOUT) {
		WriteDataToSocket(/*isasync=*/ true);
	}

	if (events & EPOLLERR || events & EPOLLHUP) {
		/*
		 * Connection encountered an error. Either the client hung up or there
		 * is network error. Fail all ops.
		 */
		DEADEND
	}
}

void
TCPChannel::FailOps()
{
	ASSERT(lock_.IsOwner());

	if (rpending_.h_) {
		rpending_.h_.Wakeup(/*status=*/ -1, rpending_.buf_);
		rpending_.Reset();
	}

	for (auto w : wpending_) {
		w.h_.Wakeup(/*status=*/ -1, w.buf_);
	}
	wpending_.clear();
}

int
TCPChannel::ReadDataFromSocket(const bool isasync)
{
	INVARIANT(lock_.IsOwner());

	if (!rpending_.buf_) {
		INVARIANT(!rpending_.bytesRead_);
		return -1;
	}

	while (true)
	{
		ASSERT(rpending_.bytesRead_ < rpending_.buf_.Size());
		uint8_t * p = rpending_.buf_.Ptr() + rpending_.bytesRead_;
		size_t size = rpending_.buf_.Size() - rpending_.bytesRead_;

		int status = recv(fd_, p, size, rpending_.isPeek_ ? MSG_PEEK : 0);

		if (status == -1) {
			if (errno == EAGAIN) {
				/*
				 * Transient error, try again
				 */
				return false;
			}

			LOG_ERROR << "Error reading from socket. " << strerror(errno);

			/*
			 * notify error and return
			 */
			if (isasync) {
				rpending_.h_.Wakeup(/*status=*/ -1, IOBuffer());
			}

			return -1;
		}

		statReadSize_.Update(status);

		if (status == 0) {
			/*
			 * no bytes were read
			 */
			break;
		}

		DEFENSIVE_CHECK(status);
		DEFENSIVE_CHECK(rpending_.bytesRead_ + status <= rpending_.buf_.Size());

		rpending_.bytesRead_ += status;

		if (rpending_.bytesRead_ == rpending_.buf_.Size()) {
			if (isasync) {
				rpending_.h_.Wakeup((int) rpending_.bytesRead_, rpending_.buf_);
			}

			int retstatus = rpending_.bytesRead_;

			rpending_.Reset();

			return retstatus;
		}
	}

	INVARIANT(rpending_.buf_);

	return rpending_.bytesRead_;
}

int
TCPChannel::WriteDataToSocket(const bool isasync)
{
	INVARIANT(lock_.IsOwner());

	int bytesWritten = 0;

	while (true) {
		if (wpending_.empty()) {
			/*
			 * nothing to write
			 */
			break;
		}

		unsigned int iovlen = wpending_.size() > IOV_MAX ? IOV_MAX : wpending_.size();
		iovec iovecs[iovlen];

		unsigned int i = 0;
		for (auto it = wpending_.begin(); it != wpending_.end(); ++it) {
			IOBuffer & data = it->buf_;
			iovecs[i].iov_base = data.Ptr();
			iovecs[i].iov_len = data.Size();

			++i;

			if (i >= iovlen) {
				break;
			}
		}

		/*
		 * write the data out to socket
		 */
		int status = writev(fd_, iovecs, iovlen);

		if (status == -1) {
			if (errno == EAGAIN) {
				/*
				 * transient failure, try again
				 */
				continue;
			}

			LOG_ERROR << "Error writing. " << strerror(errno);

			/*
			 * notify error to client
			 */
			if (isasync) {
				WriteCtx & wctx = wpending_.front();
				wctx.h_.Wakeup(/*status=*/ -1, wctx.buf_);
			}

			return -1;
		}

		statWriteSize_.Update(status);

		if (status == 0) {
		    /*
		     * no bytes written
		     */
		    break;
		}

		bytesWritten += status;

		/*
		 * trim the buffer
		 */
		uint32_t bytes = status;
		while (true) {
			ASSERT(!wpending_.empty());

			if (bytes >= wpending_.front().buf_.Size()) {
				WriteCtx wctx = wpending_.front();
				wpending_.pop_front();
				bytes -= wctx.buf_.Size();

				if (isasync) {
					wctx.h_.Wakeup(wctx.buf_.Size(), wctx.buf_);
				}

				if (bytes == 0) break;
			} else {
				wpending_.front().buf_.Cut(bytes);
				bytes = 0;
				break;
			}
		}
	}

	return bytesWritten;
}

//................................................................................... TCPServer ....

int
TCPServer::Accept(const SocketAddress & addr, const AcceptDoneHandle & h)
{
	sockaddr_in saddr = addr.LocalAddr(); 

	Guard _(&lock_);

	h_ = h;

	sockfd_ = socket(AF_INET, SOCK_STREAM, 0);

	if (sockfd_ < 0) {
		LOG_ERROR << "Socket error." << strerror(errno);
		return -1;
	}

	int status = fcntl(sockfd_, F_SETFL, O_NONBLOCK);

	if (status != 0) {
	    LOG_ERROR << "Socket error." << strerror(errno);
	    return -1;
	}

	status = ::bind(sockfd_, (struct sockaddr *) &saddr, sizeof(sockaddr_in));

	if (status != 0) {
	    LOG_ERROR << "Error binding socket. " << strerror(errno);
	    return -1;
	}

	SocketOptions::SetTcpNoDelay(sockfd_, /*enable=*/ true);
#if 0
	SocketOptions::SetTcpWindow(sockfd_, /*size=*/ 85 * 1024);
#endif

	status = listen(sockfd_, MAXBACKLOG);

	if (status != 0) {
		LOG_ERROR << "Error listening. " << strerror(errno);
		return -1;
	}

	const bool ok = epoll_.Add(sockfd_, EPOLLIN,
				   intr_fn(this, &TCPServer::HandleFdEvent));

	if (!ok) {
		LOG_ERROR << "Error registering socket with epoll.";
		return -1;
	}

	LOG_INFO << "TCP Server started. ";

	return 0;
}

void
TCPServer::HandleFdEvent(int fd, uint32_t events)
{
	Guard _(&lock_);

	INVARIANT(events == EPOLLIN);
	INVARIANT(fd == sockfd_);

	sockaddr_in addr;
	socklen_t len = sizeof(sockaddr_in);
	memset(&addr, 0, len);

	int clientfd = accept4(sockfd_, (sockaddr *) &addr, &len, SOCK_NONBLOCK);
	if (clientfd == -1) {
		/*
		 * error accepting connection, return error to client
		 */
		LOG_ERROR << "Error accepting client connection. " << strerror(errno);
		h_.Wakeup(/*status=*/ -1, static_cast<UnicastTransportChannel *>(NULL));
		return;
	}

	DEFENSIVE_CHECK(clientfd != -1);

	/*
	 * Accepted. Create a channel object and return to client
	 */
	UnicastTransportChannel * ch = new TCPChannel(fqn(clientfd), clientfd, epoll_);
	INVARIANT(ch);

	h_.Wakeup(/*status=*/ 0, ch);

	LOG_DEBUG << "Accepted. clientfd=" << clientfd;
}

int
TCPServer::Stop(const StopDoneHandle & h)
{
	Guard _(&lock_);

	/*
	 * unregister from epoll so no new connections are delivered
	 */
	const bool ok = epoll_.Remove(sockfd_);
	INVARIANT(ok);

	/*
	 * Tear down the socket safely
	 */
	::shutdown(sockfd_, SHUT_RDWR);
	::close(sockfd_);

	/*
	 * Drain pending notifictions
	 */
	ThreadPool::Schedule(this, &This::BarrierDone, h);
	return 0;
}

void
TCPServer::BarrierDone(StopDoneHandle h)
{
	h.Wakeup(/*status=*/ 0);
}

//................................................................................ TCPConnector ....

int
TCPConnector::Connect(const SocketAddress & addr, const ConnectDoneHandle & h)
{
	int fd = socket(AF_INET, SOCK_STREAM, 0);
	INVARIANT(fd >= 0);

	const int enable = 1;
	int status = setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable));
	INVARIANT(status == 0);

	status = fcntl(fd, F_SETFL, O_NONBLOCK);
	INVARIANT(status == 0);

	status = SocketOptions::SetTcpNoDelay(fd, /*enable=*/ false);
	INVARIANT(status);

	// status = SocketOptions::SetTcpWindow(fd, /*size=*/ 640 * 1024);
	// INVARIANT(status);

	status = ::bind(fd, (sockaddr *) &addr.LocalAddr(), sizeof(sockaddr_in));
	INVARIANT(status == 0);

	status = connect(fd, (sockaddr *) &addr.RemoteAddr(), sizeof(sockaddr_in));
	INVARIANT(status == -1 && errno == EINPROGRESS);

	{
		Guard _(&lock_);
		INVARIANT(pendingConnects_.find(fd) == pendingConnects_.end());
		const bool ok = pendingConnects_.insert(make_pair(fd, h)).second;
		INVARIANT(ok);
	}

	const bool ok = epoll_.Add(fd, EPOLLOUT, intr_fn(this, &TCPConnector::HandleFdEvent));
	INVARIANT(ok);

	return 0;
}

void
TCPConnector::HandleFdEvent(int fd, uint32_t events)
{
	LOG_INFO << "connected: events=" << events << " fd=" << fd;

	/*
	 * Remove the connector from polling list
	 */
	const bool ok = epoll_.Remove(fd);
	INVARIANT(ok);

	/*
	 * Drop connection from the pending list
	 */
	ConnectDoneHandle h;

	{
		Guard _(&lock_);

		auto it = pendingConnects_.find(fd);
		INVARIANT(it != pendingConnects_.end());
		h = it->second;
		pendingConnects_.erase(it);
	}

	/*
	 * Notify client
	 */
	if (events == EPOLLOUT) {
		/*
		 * Channel was established
		 */
		LOG_DEBUG << "TCP Client connected. fd=" << fd;

		TCPChannel * ch = new TCPChannel("/tcp/ch/" + STR(fd), fd, epoll_);
		h.Wakeup(/*status=*/ 0, ch); 
		return;
	}

	/*
	 * failed to connect to the specified server
	 */
	INVARIANT(events & EPOLLERR);

	LOG_ERROR << "Failed to connect. fd=" << fd << " errno=" << errno;

	h.Wakeup(/*status=*/ -1, /*ch=*/ NULL);
}

int
TCPConnector::Stop(const StopDoneHandle & h)
{
	Guard _(&lock_);

	/*
	 * Unplug from epoll
	 */
	for (auto conn : pendingConnects_) {
		const int & fd = conn.first;

		/*
		 * remove client from epoll
		 */
		const bool ok = epoll_.Remove(fd);
		INVARIANT(ok);

		/*
		 * Close client socket
		 */
		::close(fd);
		::shutdown(fd, SHUT_RDWR);
	}

	/*
	 * Schedule a barrier event
	 */
	 ThreadPool::Schedule(this, &This::BarrierDone, h);

	return 0;
}

void
TCPConnector::BarrierDone(StopDoneHandle h)
{
	Guard _(&lock_);

	/*
	 * Notify error to all pending connects
	 */
	 for (auto conn : pendingConnects_) {
		ConnectDoneHandle & connh = conn.second;
		connh.Wakeup(/*status=*/ -1, /*ch=*/ NULL);
	 }

	 pendingConnects_.clear();

	 h.Wakeup(/*status=*/ 0);
}

