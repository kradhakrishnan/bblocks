#include "net/tcp-linux.h"


using namespace std;
using namespace dh_core;

//.............................................................. TCPChannel ....

TCPChannel::TCPChannel(const std::string & name, int fd, Epoll & epoll)
    : log_(name)
    , lock_(name)
    , fd_(fd)
    , epoll_(epoll)
    /* Perf Counters */
    , statReadSize_("stat/read-io", "bytes", PerfCounter::BYTES)
    , statWriteSize_("stat/write-io", "bytes", PerfCounter::BYTES)
{
    ASSERT(fd_ >= 0);
}

TCPChannel::~TCPChannel()
{
    INVARIANT(!client_.h_);

    INFO(log_) << statReadSize_;
    INFO(log_) << statWriteSize_;
}

int
TCPChannel::EnqueueWrite(const IOBuffer & data)
{
    ASSERT(data);

    Guard _(&lock_);

    /*
     * DEPRECATED : Please use Write(const IOBuffer &, const WriteDoneHandler &) variant
     */

    if (wbuf_.size() > DEFAULT_WRITE_BACKLOG) {
        // Reached backlog limits, need to reject the write
        return -EBUSY;
    }

    if (wbuf_.empty()) {
        // No backlog. We should try to process the write synchronously
        // first
        wbuf_.push_back(WriteCtx(data, client_.writeDoneHandler_));
        return WriteDataToSocket(/*isasync=*/ false);
    }

    DEFENSIVE_CHECK(!wbuf_.empty() && wbuf_.size() <= DEFAULT_WRITE_BACKLOG);

    wbuf_.push_back(WriteCtx(data, client_.writeDoneHandler_));
    WriteDataToSocket(/*isasync=*/ true);

    return 0;
}

int
TCPChannel::Write(const IOBuffer & buf, const WriteDoneHandler & h)
{
	ASSERT(buf);

	Guard _(&lock_);

	if (wbuf_.empty()) {
		/*
		 * There is no backlog, trying writing synchronously
		 */
		 wbuf_.push_back(WriteCtx(buf, h));
		 return WriteDataToSocket(/*isasync=*/ false);
	}

	wbuf_.push_back(WriteCtx(buf, h));
	return WriteDataToSocket(/*isasync=*/ this);
}

bool
TCPChannel::Read(IOBuffer & data, const ReadDoneHandler & chandler)
{
    Guard _(&lock_);

    // TODO: Add IOBuffer to the return so we know that read client is not
    // misbehaving
    ASSERT(!rctx_.buf_ && !rctx_.bytesRead_);
    ASSERT(data);
    rctx_ = ReadCtx(data, chandler);

    return ReadDataFromSocket(/*isasync=*/ false);
}

void
TCPChannel::RegisterHandle(CHandle * h)
{
    {
        Guard _(&lock_);

        INVARIANT(h && !client_.h_);
        client_.h_ = h;
    }

    const bool ok= epoll_.Add(fd_, EPOLLIN | EPOLLOUT | EPOLLET,
                              intr_fn(this, &TCPChannel::HandleFdEvent));
    INVARIANT(ok);
}

void
TCPChannel::UnregisterHandle(CHandle * h, const UnregisterDoneFn cb)
{
    ASSERT(h)
    ASSERT(cb);

    {
        Guard _(&lock_);
        INVARIANT(client_.h_ == h);
        client_.unregisterDoneFn_ = cb;
    }

    // remove fd
    const bool status = epoll_.Remove(fd_);
    INVARIANT(status);

    ThreadPool::ScheduleBarrier(this, &TCPChannel::BarrierDone, /*nonce=*/ 0);
}

void
TCPChannel::BarrierDone(int)
{
    Client c = client_;

    {
        Guard _(&lock_);

        // at this point we should have no more code in TCPChannel
        // clear all buffer, reset client

        wbuf_.clear();
        rctx_ = ReadCtx();
        client_ = Client();
    }

    (c.h_->*c.unregisterDoneFn_)(/*status=*/ 0);
}

void
TCPChannel::Close()
{
    INVARIANT(!client_.h_);

    DEBUG(log_) << "Closing channel " << fd_;

    ::shutdown(fd_, SHUT_RDWR);
    ::close(fd_);
}

void
TCPChannel::HandleFdEvent(int fd, uint32_t events)
{
    ASSERT(fd == fd_);
    ASSERT(!(events & ~(EPOLLIN | EPOLLOUT)));

    DEBUG(log_) << "Epoll Notification: fd=" << fd_
                << " events:" << events;

    Guard _(&lock_);

    if (events & EPOLLIN) {
        ReadDataFromSocket(/*isasync=*/ true);
    }

    if (events & EPOLLOUT) {
        WriteDataToSocket(/*isasync=*/ true);
    }
}

bool
TCPChannel::ReadDataFromSocket(const bool isasync)
{
    INVARIANT(lock_.IsOwner());

    if (!rctx_.buf_) {
        INVARIANT(!rctx_.bytesRead_);
        return false;
    }

    while (true)
    {
        ASSERT(rctx_.bytesRead_ < rctx_.buf_.Size());
        uint8_t * p = rctx_.buf_.Ptr() + rctx_.bytesRead_;
        size_t size = rctx_.buf_.Size() - rctx_.bytesRead_;

        int status = read(fd_, p, size);

        if (status == -1) {
            if (errno == EAGAIN) {
                // Transient error, try again
                return false;
            }

            ERROR(log_) << "Error reading from socket.";

            // notify error and return
            rctx_.chandler_.Wakeup(this, /*status=*/ -1, IOBuffer());
            return false;
        }

        DEFENSIVE_CHECK(lock_.IsOwner());

        statReadSize_.Update(status);

        if (status == 0) {
            // no bytes were read
            break;
        }

        DEFENSIVE_CHECK(status);
        DEFENSIVE_CHECK(rctx_.bytesRead_ + status <= rctx_.buf_.Size());

        rctx_.bytesRead_ += status;

        if (rctx_.bytesRead_ == rctx_.buf_.Size()) {
            auto rctx = rctx_;
            rctx_.Reset();
            if (isasync) {
                // We need to respond since we are called in async context
                rctx.chandler_.Wakeup(this, (int) rctx.bytesRead_, rctx.buf_);
            }

            //
            // Watch out, we don't own the lock here
            //
            return true;
        }
    }

    INVARIANT(rctx_.buf_);

    return false;
}

size_t
TCPChannel::WriteDataToSocket(const bool isasync)
{
    INVARIANT(lock_.IsOwner());

    int bytesWritten = 0;

    while (true) {
        if (wbuf_.empty()) {
            // nothing to write
            break;
        }

        // construct iovs
        unsigned int iovlen = wbuf_.size() > IOV_MAX ? IOV_MAX : wbuf_.size();
        iovec iovecs[iovlen];
        unsigned int i = 0;
        for (auto it = wbuf_.begin(); it != wbuf_.end(); ++it) {
            IOBuffer & data = it->buf_;
            iovecs[i].iov_base = data.Ptr();
            iovecs[i].iov_len = data.Size();

            ++i;

            if (i >= iovlen) {
                break;
            }
        }

        // write the data out to socket
        ssize_t status = writev(fd_, iovecs, iovlen);

        if (status == -1) {
            if (errno == EAGAIN) {
                // transient failure, try again
                continue;
            }

            ERROR(log_) << "Error writing. " << strerror(errno);

            // notify error to client
            if (isasync) {
                client_.writeDoneHandler_.Wakeup(this, /*status=*/ -1);
            }
            return -1;
        }

        DEFENSIVE_CHECK(lock_.IsOwner());

        statWriteSize_.Update(status);

        if (status == 0) {
            // no bytes written
            break;
        }

        bytesWritten += status;

        // trim the buffer
        uint32_t bytes = status;
        while (true) {
            ASSERT(!wbuf_.empty());

            if (bytes >= wbuf_.front().buf_.Size()) {
		WriteCtx wctx = wbuf_.front();
		wbuf_.pop_front();
                bytes -= wctx.buf_.Size();

	        if (isasync) {
		    wctx.h_.Wakeup(this, wctx.buf_.Size());
		}

                if (bytes == 0) {
                    break;
                }
            } else {
                wbuf_.front().buf_.Cut(bytes);
                bytes = 0;
                break;
            }
        }
    }

    return bytesWritten;
}


//............................................................... TCPServer ....

bool
TCPServer::Listen(const sockaddr_in saddr, const ConnectHandler & chandler)
{
    Guard _(&lock_);

    client_ = chandler;

    sockfd_ = socket(AF_INET, SOCK_STREAM, 0);

    if (sockfd_ < 0) {
        ERROR(log_) << "Socket error." << strerror(errno);
        return false;
    }

    int status = fcntl(sockfd_, F_SETFL, O_NONBLOCK);

    if (status != 0) {
        ERROR(log_) << "Socket error." << strerror(errno);
        return false;
    }

    status = ::bind(sockfd_, (struct sockaddr *) &saddr, sizeof(sockaddr_in));

    if (status != 0) {
        ERROR(log_) << "Error binding socket. " << strerror(errno);
        return false;
    }

#if 0
    SocketOptions::SetTcpNoDelay(sockfd_, /*enable=*/ true);
    SocketOptions::SetTcpWindow(sockfd_, /*size=*/ 85 * 1024);
#endif

    status = listen(sockfd_, MAXBACKLOG);

    if (status != 0) {
        ERROR(log_) << "Error listening. " << strerror(errno);
        return false;
    }

    const bool ok = epoll_.Add(sockfd_, EPOLLIN,
                               intr_fn(this, &TCPServer::HandleFdEvent));

    if (!ok) {
        ERROR(log_) << "Error registering socket with epoll.";
        return false;
    }

    INFO(log_) << "TCP Server started. ";

    return true;
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
        //
        // error accepting connection, return error to client
        //
        ERROR(log_) << "Error accepting client connection. " << strerror(errno);
        client_.Wakeup(this, /*status=*/ -1, static_cast<TCPChannel *>(NULL));
        return;
    }

    DEFENSIVE_CHECK(clientfd != -1);

    //
    // Accepted. Create a channel object and return to client
    //
    TCPChannel * ch = new TCPChannel(TCPChannelLogPath(clientfd),
                                     clientfd, epoll_);
    INVARIANT(ch);

    client_.Wakeup(this, /*status=*/ 0, ch);

    DEBUG(log_) << "Accepted. clientfd=" << clientfd;
}

void
TCPServer::Shutdown()
{
    //
    // unregister from epoll so no new connections are delivered
    //
    const bool ok = epoll_.Remove(sockfd_);
    INVARIANT(ok);

    //
    // Tear down the socket safely
    //
    Guard _(&lock_);

    ::shutdown(sockfd_, SHUT_RDWR);
    ::close(sockfd_);
}

//............................................................ TCPConnector ....

void
TCPConnector::Connect(const SocketAddress addr, const ConnectHandler & chandler)
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    INVARIANT(fd >= 0);

    const int enable = 1;
    int status = setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &enable,
                            sizeof(enable));
    INVARIANT(status == 0);

    status = fcntl(fd, F_SETFL, O_NONBLOCK);
    INVARIANT(status == 0);

    status = SocketOptions::SetTcpNoDelay(fd, /*enable=*/ false);
    INVARIANT(status);

    status = SocketOptions::SetTcpWindow(fd, /*size=*/ 640 * 1024);
    INVARIANT(status);

    status = ::bind(fd, (sockaddr *) &addr.LocalAddr(), sizeof(sockaddr_in));
    INVARIANT(status == 0);

    status = connect(fd, (sockaddr *) &addr.RemoteAddr(), sizeof(sockaddr_in));
    INVARIANT(status == -1 && errno == EINPROGRESS);

    {
        Guard _(&lock_);
        INVARIANT(clients_.insert(make_pair(fd, chandler)).second);
    }

    const bool ok = epoll_.Add(fd, EPOLLOUT,
                               intr_fn(this, &TCPConnector::HandleFdEvent));
    INVARIANT(ok);
}

void
TCPConnector::Shutdown()
{
    Guard _(&lock_);

    INFO(log_) << "Closing TCP client. ";

    for (auto client : clients_) {
        const int & fd = client.first;
        ConnectHandler & chandler =  client.second;
        //
        // remove client from epoll
        //
        const bool ok = epoll_.Remove(fd);
        INVARIANT(ok);

        //
        // Close client socket
        //
        ::close(fd);
        ::shutdown(fd, SHUT_RDWR);

        //
        // Notify error to client
        //
        chandler.Wakeup(this, /*status=*/ -1, static_cast<TCPChannel *>(NULL));
    }

    clients_.clear();
}

void
TCPConnector::HandleFdEvent(int fd, uint32_t events)
{
    INFO(log_) << "connected: events=" << events << " fd=" << fd;

    //
    // Remove the connector from polling list
    //
    const bool ok = epoll_.Remove(fd);
    INVARIANT(ok);

    //
    // Drop client from the clients list
    //
    ConnectHandler client;

    {
        Guard _(&lock_);

        auto it = clients_.find(fd);
        INVARIANT(it != clients_.end());
        client = it->second;
        clients_.erase(it);
    }

    //
    // Notify client
    // 
    if (events == EPOLLOUT) {
        //
        // Channel was established
        //
        DEBUG(log_) << "TCP Client connected. fd=" << fd;

        TCPChannel * ch = new TCPChannel(TCPChannelLogPath(fd), fd, epoll_);
        client.Wakeup(this, /*status=*/ 0, ch); 
        return;
    }

    //
    // failed to connect to the specified server
    //
    INVARIANT(events & EPOLLERR);

    ERROR(log_) << "Failed to connect. fd=" << fd << " errno=" << errno;

    client.Wakeup(this, /*status=*/ -1, static_cast<TCPChannel *>(NULL));
}
