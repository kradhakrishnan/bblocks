#include "core/net/tcp-linux.h"


using namespace std;
using namespace dh_core;

//.............................................................. TCPChannel ....

int
TCPChannel::EnqueueWrite(const IOBuffer & data)
{
    {
        AutoLock _(&lock_);

        if (wbuf_.Size() > DEFAULT_WRITE_BACKLOG) {
            return -EBUSY;
        }

        if (!wbuf_.IsEmpty()) {
            wbuf_.Push(data);
            return WriteDataToSocket(/*isasync=*/ false);
        }

        wbuf_.Push(data);
        WriteDataToSocket(/*isasync=*/ true);
    }


    return 0;
}

bool
TCPChannel::Read(IOBuffer & data, const ReadDoneFn fn)
{
    {
        AutoLock _(&lock_);

        // TODO: Add IOBuffer to the return so we know that read client is not
        // misbehaving
        ASSERT(!rctx_.buf_ && !rctx_.bytesRead_);
        ASSERT(data);
        rctx_ = ReadCtx(data, fn);

        return ReadDataFromSocket(/*isasync=*/ false);
    }

}

void
TCPChannel::RegisterHandle(CHandle * h)
{
    {
        AutoLock _(&lock_);

        INVARIANT(h && !client_.h_);
        client_.h_ = h;
    }

    epoll_->Add(fd_, EPOLLIN|EPOLLOUT|EPOLLET, this,
                async_fn(&TCPChannel::HandleFdEvent));
}

void
TCPChannel::UnregisterHandle(CHandle * h, const UnregisterDoneFn cb)
{
    ASSERT(h)
    ASSERT(cb);

    {
        AutoLock _(&lock_);
        INVARIANT(client_.h_ == h);
        client_.unregisterDoneFn_ = cb;
    }

    // remove fd
    epoll_->Remove(fd_);

    ThreadPool::ScheduleBarrier(make_cb(this, &TCPChannel::BarrierDone));
}

void
TCPChannel::BarrierDone(int)
{
    Client c = client_;

    {
        AutoLock _(&lock_);

        // at this point we should have no more code in TCPChannel
        // clear all buffer, reset client

        wbuf_.Clear();
        rctx_ = ReadCtx();
        client_ = Client();
    }

    (c.h_->*c.unregisterDoneFn_)(/*status=*/ 0);
}

void
TCPChannel::Close()
{
    ASSERT(!client_.h_);

    DEBUG(log_) << "Closing channel " << fd_;

    ::shutdown(fd_, SHUT_RDWR);
    ::close(fd_);

    epoll_ = NULL;
}

void
TCPChannel::HandleFdEvent(Epoll *, int fd, uint32_t events)
{
    ASSERT(fd == fd_);
    ASSERT(!(events & ~(EPOLLIN | EPOLLOUT)));

    DEBUG(log_) << "Epoll Notification: fd=" << fd_
                << " events:" << events;

    AutoLock _(&lock_);

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
            ThreadPool::Schedule(client_.h_, rctx_.fn_, this, /*status=*/ -1,
                                 IOBuffer());
            return false;
        }

        if (status == 0) {
            // no bytes were read
            break;
        }

        ASSERT(status);
        ASSERT(rctx_.bytesRead_ + status <= rctx_.buf_.Size());
        rctx_.bytesRead_ += status;

        if (rctx_.bytesRead_ == rctx_.buf_.Size()) {
            if (isasync) {
                ThreadPool::Schedule(client_.h_, rctx_.fn_, this,
                                     (int) rctx_.bytesRead_, rctx_.buf_);
            }
            rctx_.Reset();
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
        if (wbuf_.IsEmpty()) {
            // nothing to write
            break;
        }

        // construct iovs
        unsigned int iovlen = wbuf_.Size() > IOV_MAX ? IOV_MAX : wbuf_.Size();
        iovec iovecs[iovlen];
        unsigned int i = 0;
        for (auto it = wbuf_.Begin(); it != wbuf_.End(); ++it) {
            IOBuffer & data = *it;
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
                // transient failure
                break;
            }

            ERROR(log_) << "Error writing. " << strerror(errno);

            // notify error to client
            if (isasync) {
                ThreadPool::Schedule(client_.h_, client_.writeDoneFn_,
                                     this, /*status=*/ -1);
            }
            return -1;
        }

        if (status == 0) {
            // no bytes written
            break;
        }

        bytesWritten += status;

        // trim the buffer
        uint32_t bytes = status;
        while (true) {
            ASSERT(!wbuf_.IsEmpty());

            if (bytes >= wbuf_.Front().Size()) {
                IOBuffer data = wbuf_.Pop();
                bytes -= data.Size();

                if (bytes == 0) {
                    break;
                }
            } else {
                wbuf_.Front().Cut(bytes);
                bytes = 0;
                break;
            }
        }
    }

    if (isasync && bytesWritten != 0) {
        ThreadPool::Schedule(client_.h_, client_.writeDoneFn_, this,
                                 bytesWritten);
    }

    return bytesWritten;
}


//............................................................... TCPServer ....

void
TCPServer::Listen(CHandle * h, const sockaddr_in saddr, const ConnDoneFn cb)
{
    ASSERT(h);
    ASSERT(cb);

    AutoLock _(&lock_);

    INVARIANT(!client_.h_);

    client_.h_ = h;
    client_.connDoneFn_ = cb;

    sockfd_ = socket(AF_INET, SOCK_STREAM, 0);
    ASSERT(sockfd_ >= 0);

    int status = fcntl(sockfd_, F_SETFL, O_NONBLOCK);
    INVARIANT(status == 0);

    status = ::bind(sockfd_, (struct sockaddr *) &saddr, sizeof(sockaddr_in));
    ASSERT(status == 0);

#if 0
    SocketOptions::SetTcpNoDelay(sockfd_, /*enable=*/ true);
    SocketOptions::SetTcpWindow(sockfd_, /*size=*/ 85 * 1024);
#endif

    status = listen(sockfd_, MAXBACKLOG);
    INVARIANT(status == 0);

    epoll_->Add(sockfd_, EPOLLIN, this,
                async_fn(&TCPServer::HandleFdEvent));

    INFO(log_) << "TCP Server started. ";
}

void
TCPServer::HandleFdEvent(Epoll *, int fd, uint32_t events)
{
    AutoLock _(&lock_);

    ASSERT(events == EPOLLIN);
    ASSERT(fd == sockfd_);

    sockaddr_in addr;
    socklen_t len = sizeof(sockaddr_in);
    memset(&addr, 0, len);
    int clientfd = accept4(sockfd_, (sockaddr *) &addr, &len, SOCK_NONBLOCK);

    if (clientfd == -1) {
        // error accepting connection
        ERROR(log_) << "Error accepting client connection. " << strerror(errno);
        // notify the client of the failure
        ThreadPool::Schedule(client_.h_, client_.connDoneFn_, this,
                             /*status=*/ -1, static_cast<TCPChannel *>(NULL));
        return;
    }

    ASSERT(clientfd != -1);

    TCPChannel * ch;
    ch = new TCPChannel(TCPChannelLogPath(clientfd), clientfd, epoll_);

    // notify the client
    ThreadPool::Schedule(client_.h_, client_.connDoneFn_, this,
                         /*status=*/ 0, ch);

    DEBUG(log_) << "Accepted. clientfd=" << clientfd;
}

void
TCPServer::Shutdown()
{
    // unregister from epoll so no new connections are delivered
    epoll_->Remove(sockfd_);

    AutoLock _(&lock_);

    // reset the client
    client_ = Client();

    ::shutdown(sockfd_, SHUT_RDWR);
    ::close(sockfd_);

    epoll_ = NULL;
}

//............................................................ TCPConnector ....

__async_operation__ void
TCPConnector::Connect(const SocketAddress addr, CHandle * h, const ConnDoneFn cb)
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    assert(fd >= 0);

    const int enable = 1;
    int status = setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &enable,
                            sizeof(enable));
    INVARIANT(status == 0);

    status = fcntl(fd, F_SETFL, O_NONBLOCK);
    ASSERT(status == 0);

    status = SocketOptions::SetTcpNoDelay(fd, /*enable=*/ false);
    ASSERT(status);
    status = SocketOptions::SetTcpWindow(fd, /*size=*/ 640 * 1024);
    ASSERT(status);

    status = ::bind(fd, (sockaddr *) &addr.LocalAddr(), sizeof(sockaddr_in));
    ASSERT(status == 0);

    status = connect(fd, (sockaddr *) &addr.RemoteAddr(), sizeof(sockaddr_in));
    ASSERT(status == -1 && errno == EINPROGRESS);

    {
        AutoLock _(&lock_);
        INVARIANT(clients_.insert(make_pair(fd, Client(h, cb))).second);
    }

    epoll_->Add(fd, EPOLLOUT, this, async_fn(&TCPConnector::HandleFdEvent));
}

void
TCPConnector::Shutdown()
{
    AutoLock _(&lock_);

    INFO(log_) << "Closing TCP client. ";

    for (auto it = clients_.begin(); it != clients_.end(); ++it) {
        const int & fd = it->first;
        const Client & client =  it->second;
        // remove client from epoll
        epoll_->Remove(it->first);
        // send notification to client
        ThreadPool::Schedule(client.h_, client.connDoneFn_, this, -1,
                             static_cast<TCPChannel *>(NULL));

        ::close(fd);
        ::shutdown(fd, SHUT_RDWR);
    }

    clients_.clear();

    epoll_ = NULL;
}

void
TCPConnector::HandleFdEvent(Epoll *, int fd, uint32_t events)
{
    INFO(log_) << "connected: events=" << events << " fd=" << fd;

    epoll_->Remove(fd);

    Client client;

    {
        AutoLock _(&lock_);
        auto it = clients_.find(fd);
        INVARIANT(it != clients_.end());
        client = it->second;
        clients_.erase(it);
    }

    if (events == EPOLLOUT) {
        DEBUG(log_) << "TCP Client connected. fd=" << fd;

        TCPChannel * ch = new TCPChannel(TCPChannelLogPath(fd), fd, epoll_);
        // callback and drop from tracking list
        ThreadPool::Schedule(client.h_, client.connDoneFn_, this, /*status=*/ 0,
                             ch); 
        return;
    }

    // failed to connect to the specified server
    ASSERT(events & EPOLLERR);

    ERROR(log_) << "Failed to connect. fd=" << fd << " errno=" << errno;

    ThreadPool::Schedule(client.h_, client.connDoneFn_, this, /*status=*/ -1,
                         static_cast<TCPChannel *>(NULL));
}
