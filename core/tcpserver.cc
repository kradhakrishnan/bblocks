#include "core/tcpserver.h"

#include <fcntl.h>

using namespace std;
using namespace dh_core;

//.............................................................. TCPChannel ....

bool
TCPChannel::EnqueueWrite(const IOBuffer & data)
{
    {
        AutoLock _(&lock_);

        if (wbuf_.Size() > DEFAULT_WRITE_BACKLOG) {
            return false;
        }

        wbuf_.Push(data);
    }

    WriteDataToSocket();

    return true;
}

void
TCPChannel::Read(IOBuffer & data)
{
    {
        AutoLock _(&lock_);

        // TODO: Add IOBuffer to the return so we know that read client is not
        // misbehaving
        ASSERT(!rctx_.buf_ && !rctx_.bytesRead_);
        ASSERT(data);
        rctx_ = ReadCtx(data);
    }

    ReadDataFromSocket();
}

void
TCPChannel::RegisterClient(TCPChannelClient * client)
{
    ASSERT(!client_);
    ASSERT(client);

    client_ = client;

    epoll_->Add(fd_, EPOLLIN|EPOLLOUT|EPOLLET, this, NULL);
}

void
TCPChannel::UnregisterClient(TCPChannelClient *, Callback<int> *)
{
    DEADEND
}

void
TCPChannel::Close()
{
    ASSERT(!client_);

    DEBUG(log_) << "Closing channel " << fd_;

    epoll_->Remove(fd_, /*cb=*/ NULL);
}

void
TCPChannel::EpollSetHandleFdEvent(int fd, uint32_t events)
{
    ASSERT(fd == fd_);
    ASSERT(!(events & ~(EPOLLIN | EPOLLOUT)));

    DEBUG(log_) << "Epoll Notification: fd=" << fd_
                << " events:" << events;


    if (events & EPOLLIN) {
        ReadDataFromSocket();
    }

    if (events & EPOLLOUT) {
        WriteDataToSocket();
    }
}

void
TCPChannel::ReadDataFromSocket()
{
    AutoLock _(&lock_);

    if (!rctx_.buf_) {
        INVARIANT(!rctx_.bytesRead_);
        return;
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
                return;
            }

            ERROR(log_) << "Error reading from socket.";

            // notify error and return
            ThreadPool::Schedule(client_, &TCPChannelClient::TcpReadDone,
                                 this, /*status=*/ -1, IOBuffer());
            return;
        }

        if (status == 0) {
            // no bytes were read
            break;
        }

        ASSERT(status);
        ASSERT(rctx_.bytesRead_ + status <= rctx_.buf_.Size());
        rctx_.bytesRead_ += status;

        if (rctx_.bytesRead_ == rctx_.buf_.Size()) {
            ThreadPool::Schedule(client_, &TCPChannelClient::TcpReadDone, this,
                                 (int) rctx_.bytesRead_, rctx_.buf_);
            rctx_.Reset();
            return;
        }
    }
}

void
TCPChannel::WriteDataToSocket()
{
    AutoLock _(&lock_);

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
            ThreadPool::Schedule(client_, &TCPChannelClient::TcpWriteDone,
                                 this, /*status=*/ -1);
            return;
 
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

    if (bytesWritten != 0) {
        ThreadPool::Schedule(client_, &TCPChannelClient::TcpWriteDone, this,
                             bytesWritten);
    }
}


//............................................................... TCPServer ....

void
TCPServer::Accept(TCPServerClient * client, Callback<status_t> * cb)
{
    ASSERT(client);
    ASSERT(!client_);

    client_ = client;

    sockfd_ = socket(AF_INET, SOCK_STREAM, 0);
    ASSERT(sockfd_ >= 0);

    int status = fcntl(sockfd_, F_SETFL, O_NONBLOCK);
    INVARIANT(status == 0);

    status = bind(sockfd_, (struct sockaddr *) &servaddr_, sizeof(sockaddr_in));
    ASSERT(status == 0);

    SocketOptions::SetTcpNoDelay(sockfd_, /*enable=*/ true);
    // SocketOptions::SetTcpWindow(sockfd_, /*size=*/ 85 * 1024);

    status = listen(sockfd_, MAXBACKLOG);
    ASSERT(status == 0);

    ThreadPool::Schedule(epoll_, &EpollSet::Add, sockfd_, (unsigned int) EPOLLIN,
                         static_cast<EpollSetClient *>(this), cb);

    INFO(log_) << "TCP Server started. ";
}

void
TCPServer::EpollSetHandleFdEvent(int fd, uint32_t events)
{
    ASSERT(events == EPOLLIN);
    ASSERT(fd == sockfd_);

    sockaddr_in addr;
    socklen_t len = sizeof(sockaddr_in);
#ifdef VALGRIND_BUILD
    memset(&addr, 0, len);
#endif
    int clientfd = accept4(sockfd_, (sockaddr *) &addr, &len, SOCK_NONBLOCK);

    if (clientfd == -1) {
        // error accepting connection
        ERROR(log_) << "Error accepting client connection. " << strerror(errno);
        // notify the client of the failure
        ThreadPool::Schedule(client_, &TCPServerClient::TCPServerHandleConnection,
                             -1, /*conn=*/ static_cast<TCPChannel *>(NULL));
        return;
    }

    ASSERT(clientfd != -1);

    TCPChannel * ch;
    ch = new TCPChannel(TCPChannelLogPath(clientfd), clientfd, epoll_);

    // add the channel to the list
    iochs_.push_back(ch);

    // notify the client
    ThreadPool::Schedule(client_, &TCPServerClient::TCPServerHandleConnection,
                         /*status=*/ 0, ch);

    DEBUG(log_) << "Accepted. clientfd=" << clientfd;
}

void
TCPServer::Shutdown(Callback<int> * cb)
{
    // TODO: Remove race here by making it async
    ThreadPool::Schedule(epoll_, &EpollSet::Remove, sockfd_,
                         make_cb(this, &TCPServer::ShutdownFdRemoved, cb));
}

void
TCPServer::ShutdownFdRemoved(int status, Callback<int> * cb)
{
    ASSERT(status == OK);

    for (iochs_t::iterator it = iochs_.begin(); it != iochs_.end(); ++it) {
        TCPChannel * ch = *it;
        ch->Close();
    }
    iochs_.clear();

    ::shutdown(sockfd_, SHUT_RDWR);
    ::close(sockfd_);

    client_ = NULL;

    if (cb) cb->ScheduleCallback(OK);
}

//............................................................... TCPClient ....

void
TCPConnector::Connect(const SocketAddress addr,
                      Callback2<int, TCPChannel *> * cb)
{
    ASSERT(cb);

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

    status = bind(fd, (sockaddr *) &addr.LocalAddr(), sizeof(sockaddr_in));
    ASSERT(status == 0);

    status = connect(fd, (sockaddr *) &addr.RemoteAddr(), sizeof(sockaddr_in));
    ASSERT(status == -1 && errno == EINPROGRESS);

    {
        AutoLock _(&lock_);
        INVARIANT(clients_.insert(make_pair(fd, cb)).second);
    }

    ThreadPool::Schedule(epoll_, &EpollSet::Add, fd, (unsigned int) EPOLLOUT,
                         static_cast<EpollSetClient *>(this),
                         make_cb(this, &TCPConnector::EpollSetAddDone));
}

void
TCPConnector::EpollSetAddDone(const int status)
{
    ASSERT(status == 0);

    // TODO: Handle error
}

void
TCPConnector::Stop(Callback<int> * cb)
{
    AutoLock _(&lock_);

    INFO(log_) << "Closing TCP client. ";

    for (auto it = clients_.begin(); it != clients_.end(); ++it) {
        it->second->ScheduleCallback(-1, /*ch=*/ NULL);
        epoll_->Remove(it->first);
    }

    clients_.clear();

    if (cb) cb->ScheduleCallback(OK);
}

void
TCPConnector::EpollSetHandleFdEvent(int fd, uint32_t events)
{
    INFO(log_) << "connected: events=" << events << " fd=" << fd;

    epoll_->Remove(fd, NULL);

    Callback2<int, TCPChannel *> * cb;

    {
        AutoLock _(&lock_);
        auto it = clients_.find(fd);
        INVARIANT(it != clients_.end());
        cb = it->second;
        clients_.erase(it);
    }

    if (events == EPOLLOUT) {
        DEBUG(log_) << "TCP Client connected. fd=" << fd;

        TCPChannel * ch = new TCPChannel(TCPChannelLogPath(fd), fd, epoll_);
        // callback and drop from tracking list
        cb->ScheduleCallback(/*status=*/ 0, ch); 
        return;
    }

    // failed to connect to the specified server
    ASSERT(events & EPOLLERR);
    ERROR(log_) << "Failed to connect. fd=" << fd << " errno=" << errno;

    cb->ScheduleCallback(/*status=*/ -1, static_cast<TCPChannel *>(NULL));
}
