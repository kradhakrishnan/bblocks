#include "core/tcpserver.h"

#include <fcntl.h>

using namespace std;
using namespace kware;

static const uint32_t MAXBUFFERSIZE = 1 * 1024 * 1024; // 1 MiB

//
// TCPChannel
//
const uint32_t
TCPChannel::DEFAULT_READ_SIZE;

bool
TCPChannel::HandleAction(Actor * from, const ActorCommand & type,
                         void * data, void * ctx)
{
    ASSERT(!ctx);

    DEBUG(log_) << "HandleAction: from :" << from << " type:" << type;

    IF(EPOLL_NOTIFICATION)
        epoll_event * ee = (epoll_event *) data;
        ASSERT(epoll_.get() == from);
        ASSERT(ee->data.fd == fd_);
        HandleEpollNotification(*ee);
        SendMessage(from, EPOLL_NOTIFICATION_ACK, (void *)(uint64_t) fd_);
    ELIF(READ)
        ASSERT(!data);
        RegisterReadImpl(from);
    ELIF(WRITE)
        DataBuffer * buf = (DataBuffer *) data;
        WriteImpl(from, *buf);
        delete buf;
    ELIF(CLOSE)
        Close();
    ELSE NOTREACHED
    FI

    return true;
}

action_t
TCPChannel::Write(Actor * writer, const DataBuffer & buf)
{
    return ScheduleAction(writer, WRITE, new DataBuffer(buf));
}

void
TCPChannel::WriteImpl(Actor * writer, const DataBuffer & data)
{
    // There can be at most one writer waiting for notification any given time.
    // There cannot be any dirty buffer. Otherwise it is a misbehaving writer
    ASSERT(!writer_);
    // ASSERT(outq_.Size() < MAXBUFFERSIZE);

    writer_ = writer;
    outq_.Append(data);

    // if (outq_.Size() < MAXBUFFERSIZE) {
    //    SendMessage(writer_, WRITE_DONE, /*data=*/ NULL);
    //    writer_ = NULL;
    //}

    WriteDataToSocket();
}

action_t
TCPChannel::RegisterRead(Actor * reader)
{
    return ScheduleAction(reader, READ, /*data=*/ NULL);
}

void
TCPChannel::RegisterReadImpl(Actor * reader)
{
    ASSERT(!reader_);
    reader_ = reader;

    // Register fd for reading
    SendMessage(epoll_.get(), EPOLL_ADD_EVENT,
                new EpollSet::FdEvent(fd_, EPOLLIN|EPOLLET));
}

action_t
TCPChannel::Close(Actor * actor)
{
    return ScheduleAction(actor, CLOSE, /*data=*/ NULL);
}

void
TCPChannel::Close()
{
    CloseInternal();
}

void
TCPChannel::CloseImpl(Actor * caller)
{
    CloseInternal();
    SendMessage(caller, CLOSE_DONE, /*data=*/ NULL);
}

void
TCPChannel::CloseInternal()
{
    DEBUG(log_) << "Closing channel " << fd_;

    SendMessage(epoll_.get(), EPOLL_REMOVE_FD, new EpollSet::FdEvent(fd_));
}

void
TCPChannel::CloseInternal_EpollClosed()
{
    close(fd_);
    fd_ = -1;
}

void
TCPChannel::HandleEpollNotification(const epoll_event & eevent)
{
    DEBUG(log_) << "Epoll Notification: fd=" << fd_
                << " events:" << eevent.events;

    ASSERT(!(eevent.events & ~(EPOLLIN | EPOLLOUT)));

    if (eevent.events & EPOLLIN) {
        ReadDataFromSocket();
    }

    if (eevent.events & EPOLLOUT) {
        WriteDataToSocket();
    }

    return;
}

void
TCPChannel::ReadDataFromSocket()
{
    ASSERT(reader_);
    ASSERT(inq_.IsEmpty());

    while (true) {
        RawData data(DEFAULT_READ_SIZE);

        int status = read(fd_, data.Get(), data.Size());
        if (status == -1) {
            if (errno == EAGAIN) {
                break;
            }

            ERROR(log_) << "Error reading from socket.";
            CloseInternal();
            return;
        }

        if (status == 0) {
            break;
        }

        ASSERT(status > 0);

        uint32_t bytes = status;
        ASSERT(bytes <= data.Size());

        if (bytes < data.Size()) {
            inq_.Append(data.Cut(bytes));
        } else {
            inq_.Append(data);
        }
    }


    if (!inq_.IsEmpty()) {
        DataBuffer * data = new DataBuffer(inq_);
        SendMessage(reader_, READ_DONE, data);
        inq_.Clear();
    }
}

void
TCPChannel::WriteDataToSocket()
{
    while (true) {
        if (outq_.IsEmpty()) {
            break;
        }

        // construct iovs
        unsigned int iovlen = outq_.GetLength() > IOV_MAX ? IOV_MAX
                                                          : outq_.GetLength();
        iovec iovecs[iovlen];
        unsigned int i = 0;
        for (DataBuffer::iterator it = outq_.Begin(); it != outq_.End(); ++it) {
            RawData & data = *it;
            iovecs[i].iov_base = data.Get();
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
                break;
            }

            ERROR(log_) << "Error writing. " << strerror(errno);
            NOTREACHED
        }

        if (status == 0) {
            break;
        }

        // trim the buffer
        uint32_t bytes = status;
        while (true) {
            ASSERT(!outq_.IsEmpty());

            RawData data = outq_.Front();
            if (bytes >= data.Size()) {
                outq_.PopFront();
                bytes -= data.Size();

                if (bytes == 0) {
                    break;
                }
            } else {
                outq_.Front().Cut(bytes);
                bytes = 0;
                break;
            }
        }
    }

    if (writer_ && outq_.IsEmpty()) {
        SendMessage(writer_, WRITE_DONE, /*data=*/ NULL);
        writer_ = NULL;
    }
}

//
// TCPServer
//
bool
TCPServer::HandleAction(Actor * from, const ActorCommand & type,
                        void * data, void * ctx)
{
    IF(EPOLL_NOTIFICATION)
        epoll_event * ee = (epoll_event *) data;
        ASSERT(epoll_.get() == from);
        ASSERT(ee->data.fd == sockfd_);
        HandleEpollNotification(*ee);
        SendMessage(from, EPOLL_NOTIFICATION_ACK, (void *)(uint64_t) sockfd_);
    ELIF(TCP_ACCEPT)
        StartAcceptingImpl(from);
    ELSE
        NOTREACHED
    FI

    return true;
}

action_t
TCPServer::StartAccepting(Actor * actor)
{
    return ScheduleAction(actor, TCP_ACCEPT, actor);
}

void
TCPServer::StartAcceptingImpl(Actor * actor)
{
    ASSERT(actor);

    ASSERT(!acceptor_);
    acceptor_ = actor;

    sockfd_ = socket(AF_INET, SOCK_STREAM, 0);
    ASSERT(sockfd_ >= 0);

    int status = fcntl(sockfd_, F_SETFL, O_NONBLOCK);
    ASSERT(status == 0);

    status = bind(sockfd_, (struct sockaddr *) &servaddr_.sockaddr(),
                  sizeof(sockaddr_in));
    ASSERT(status == 0);

    SocketOptions::SetTcpNoDelay(sockfd_, /*enable=*/ false);
    SocketOptions::SetTcpWindow(sockfd_, /*size=*/ 640 * 1024);

    status = listen(sockfd_, MAXBACKLOG);
    ASSERT(status == 0);

    SendMessage(epoll_.get(), EPOLL_ADD_FD, new EpollSet::FdEvent(sockfd_,
                                                                  EPOLLIN));
    SendMessage(acceptor_, TCP_ACCEPT_STARTED);

    INFO(log_) << "TCP Server started. ";
}

void
TCPServer::HandleEpollNotification(const epoll_event & ee)
{
    ASSERT(ee.events == EPOLLIN);

    SocketAddress addr;
    socklen_t len = sizeof(sockaddr_in);
#ifdef VALGRIND_BUILD
    memset(&addr, 0, len);
#endif
    int clientfd = accept4(sockfd_, (sockaddr *) &addr.sockaddr(), &len,
                           SOCK_NONBLOCK);

    if (clientfd == -1) {
        ERROR(log_) << "Error accepting client connection. " << strerror(errno);
        SendMessage(acceptor_, ERROR, (void *)(uint64_t) errno);
        return;
    }

    const string logpath = TCPChannelLogPath(clientfd);
    TCPChannel * ch = new TCPChannel(clientfd, epoll_, logpath);
    SendMessage(epoll_.get(), ch, EPOLL_ADD_FD,
                new EpollSet::FdEvent(clientfd, EPOLLOUT));

    iochs_.push_back(ch);

    SendMessage(acceptor_, TCP_ACCEPT_DONE, ch, /*ctx=*/ NULL);

    DEBUG(log_) << "Accepted. clientfd=" << clientfd;
}

action_t
TCPServer::Shutdown()
{
    return ScheduleAction(NULL, CMD_SHUTDOWN, /*data=*/ NULL);
}

void
TCPServer::ShutdownImpl()
{
    for (iochs_t::iterator it = iochs_.begin(); it != iochs_.end(); ++it) {
        TCPChannel * ch = *it;
        ch->Close();
    }
    iochs_.clear();

    SendMessage(epoll_.get(), EPOLL_REMOVE_FD, new EpollSet::FdEvent(sockfd_));

    ::shutdown(sockfd_, SHUT_RDWR);
    ::close(sockfd_);
}

//
// TCPClient
//
bool
TCPClient::HandleAction(Actor * from, const ActorCommand & type, void * data,
                        void * ctx)
{
    IF(EPOLL_NOTIFICATION)
        ASSERT(epoll_.get() == from);
        epoll_event * ee = (epoll_event *) data;
        HandleEpollNotification(*ee);
        SendMessage(from, EPOLL_NOTIFICATION_ACK, (void *)(uint64_t) ee->data.fd);
    ELIF(TCP_CONNECT)
        SocketAddress * addr = (SocketAddress *) data;
        ConnectImpl(from, *addr);
        delete addr;
    ELSE
        NOTREACHED
    FI

    return true;
}

action_t
TCPClient::Connect(Actor * connector, const SocketAddress & addr)
{
    return ScheduleAction(connector, TCP_CONNECT, new SocketAddress(addr));
}

void
TCPClient::ConnectImpl(Actor * connector, const SocketAddress & addr)
{
    ASSERT(!connector_);
    connector_ = connector;

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    assert(fd >= 0);

    const int enable = 1;
    int status = setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &enable,
                            sizeof(enable));
    ASSERT(status == 0);

    status = fcntl(fd, F_SETFL, O_NONBLOCK);
    ASSERT(status == 0);

    status = SocketOptions::SetTcpNoDelay(fd, /*enable=*/ false);
    ASSERT(status);
    status = SocketOptions::SetTcpWindow(fd, /*size=*/ 640 * 1024);
    ASSERT(status);

    status = connect(fd, (sockaddr *) &addr.sockaddr(), sizeof(sockaddr_in));
    ASSERT(status == -1 && errno == EINPROGRESS);

    SendMessage(epoll_.get(), EPOLL_ADD_FD, new EpollSet::FdEvent(fd, EPOLLOUT));
}

void
TCPClient::HandleEpollNotification(const epoll_event & ee)
{
    const int fd = ee.data.fd;

    DEBUG(log_) << "connected: events=" << ee.events << " fd=" << fd;

    SendMessage(epoll_.get(), EPOLL_REMOVE_FD, new EpollSet::FdEvent(fd));

    if (ee.events == EPOLLOUT) {
        DEBUG(log_) << "TCP Client connected. fd=" << fd;
        // Crate and return tcp channel object
        TCPChannel * ch = new TCPChannel(fd, epoll_, TCPChannelLogPath(fd));
        // add to the list of channels under the client object
        SendMessage(epoll_.get(), ch, EPOLL_ADD_FD,
                    new EpollSet::FdEvent(fd, EPOLLOUT));
        SendMessage(connector_, TCP_CONNECT_DONE, ch);
        // reset the connector
        connector_ = NULL;
        return;
    }

    // failed to connect to the specified server
    ASSERT(ee.events & EPOLLERR);
    ERROR(log_) << "Failed to connect. fd=" << ee.data.fd;
    SendMessage(connector_, TCP_CONNECT_ERROR);

    // reset the connector
    connector_ = NULL;
}

void
TCPClient::Stop()
{
    ASSERT(!connector_);
    INFO(log_) << "Closing TCP client. ";
}
