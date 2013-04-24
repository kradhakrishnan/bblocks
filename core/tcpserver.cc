#include "core/tcpserver.h"

#include <fcntl.h>

using namespace std;
using namespace dh_core;

static const uint32_t MAXBUFFERSIZE = 1 * 1024 * 1024; // 1 MiB

//
// TCPChannel
//
const uint32_t
TCPChannel::DEFAULT_READ_SIZE;

void
TCPChannel::Write(const DataBuffer & data, Callback<int> * cb)
{
    ASSERT(cb);

    // There can be only one writer per channel at any given time
    ASSERT(!writer_cb_);
    writer_cb_ = cb;

    outq_.Append(data);

    // Flush it to socket if possible immediately
    WriteDataToSocket();
}

void
TCPChannel::RegisterRead(TCPChannel::readercb_t * cb)
{
    ASSERT(cb);

    // There can be only one reader registered per channel
    ASSERT(!reader_cb_);
    reader_cb_ = cb;

    // Register fd for reading with EpollSet
    ThreadPool::Schedule(epoll_, &EpollSet::Add, fd_, EPOLLIN|EPOLLET,
                         (EpollSetClient *) this);
}

void
TCPChannel::Close()
{
    CloseInternal();
}

void
TCPChannel::CloseInternal()
{
    DEBUG(log_) << "Closing channel " << fd_;

    // we take the hit of sync call since we don't expect much of this and to
    // keep the code simple
    epoll_->Remove(fd_);
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

    // TODO: make it async
    epoll_->NotifyHandled(fd_);
}

void
TCPChannel::ReadDataFromSocket()
{
    // pre-condition
    ASSERT(reader_cb_);
    ASSERT(inq_.IsEmpty());

    while (true) {
        RawData data(DEFAULT_READ_SIZE);

        int status = read(fd_, data.Get(), data.Size());
        if (status == -1) {
            if (errno == EAGAIN) {
                // Transient error, try again
                break;
            }

            ERROR(log_) << "Error reading from socket.";

            // do we need this ?
            CloseInternal();

            // notify error and return
            reader_cb_->ScheduleCallback(errno, /*data=*/ NULL);
            reader_cb_ = NULL;
            return;
        }

        if (status == 0) {
            // no bytes were read
            break;
        }

        uint32_t & bytes = (uint32_t &) status;

        ASSERT(bytes > 0);
        ASSERT(bytes <= data.Size());

        if (bytes < data.Size()) {
            inq_.Append(data.Cut(bytes));
        } else {
            inq_.Append(data);
        }
    }


    if (!inq_.IsEmpty()) {
        DataBuffer * data = new DataBuffer(inq_);
        reader_cb_->ScheduleCallback(/*errno=*/ 0, data);
        inq_.Clear();
    }
}

void
TCPChannel::WriteDataToSocket()
{
    while (true) {
        if (outq_.IsEmpty()) {
            // nothing to write
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
                // transient failure
                break;
            }

            ERROR(log_) << "Error writing. " << strerror(errno);
            NOTREACHED
        }

        if (status == 0) {
            // no bytes written
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

    if (writer_cb_ && outq_.IsEmpty()) {
        writer_cb_->ScheduleCallback(/*status=*/ 0);
        writer_cb_ = NULL;
    }
}

//
// TCPServer
//
void
TCPServer::StartAccepting(TCPServerClient * client)
{
    ASSERT(client);
    ASSERT(!client_);

    client_ = client;

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

    // TODO: Make it async
    epoll_->Add(sockfd_, EPOLLIN, this);

    INFO(log_) << "TCP Server started. ";
}

void
TCPServer::EpollSetHandleFdEvent(int fd, uint32_t events)
{
    ASSERT(events == EPOLLIN);

    SocketAddress addr;
    socklen_t len = sizeof(sockaddr_in);
#ifdef VALGRIND_BUILD
    memset(&addr, 0, len);
#endif
    int clientfd = accept4(sockfd_, (sockaddr *) &addr.sockaddr(), &len,
                           SOCK_NONBLOCK);

    if (clientfd == -1) {
        ERROR(log_) << "Error accepting client connection. " << strerror(errno);
        ThreadPool::Schedule(client_,
                             &TCPServerClient::TCPServerHandleConnection,
                             errno, /*conn=*/ (TCPChannel *) NULL);
        return;
    }

    const string logpath = TCPChannelLogPath(clientfd);
    TCPChannel * ch = new TCPChannel(logpath, clientfd, epoll_);

    // ThreadPool::Schedule(epoll_, &EpollSet::Add, clientfd, EPOLLOUT,
    //                     (EpollSetClient *) ch_);

    iochs_.push_back(ch);

    ThreadPool::Schedule(client_, &TCPServerClient::TCPServerHandleConnection,
                         /*status=*/ 0, ch);

    DEBUG(log_) << "Accepted. clientfd=" << clientfd;

    epoll_->NotifyHandled(sockfd_);
}

void
TCPServer::Shutdown()
{
    // TODO: Remove race here by making it async
    epoll_->Remove(sockfd_);

    for (iochs_t::iterator it = iochs_.begin(); it != iochs_.end(); ++it) {
        TCPChannel * ch = *it;
        ch->Close();
    }
    iochs_.clear();

    ::shutdown(sockfd_, SHUT_RDWR);
    ::close(sockfd_);
}

//
// TCPClient
//
void
TCPConnector::Connect(const SocketAddress & addr, TCPConnectorClient * client)
{
    ASSERT(!client_);
    client_ = client;

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

    // TODO: Make async
    epoll_->Add(fd, EPOLLOUT, this);
}

void
TCPConnector::Stop()
{
    client_ = NULL;
    INFO(log_) << "Closing TCP client. ";
}

void
TCPConnector::EpollSetHandleFdEvent(int fd, uint32_t events)
{
    DEBUG(log_) << "connected: events=" << events << " fd=" << fd;

    epoll_->Remove(fd);

    if (events == EPOLLOUT) {
        DEBUG(log_) << "TCP Client connected. fd=" << fd;
        // Crate and return tcp channel object
        TCPChannel * ch = new TCPChannel( TCPChannelLogPath(fd), fd, epoll_);
        // add to the list of channels under the client object
        // SendMessage(epoll_.get(), ch, EPOLL_ADD_FD,
        //            new EpollSet::FdEvent(fd, EPOLLOUT));
        ThreadPool::Schedule(client_,
                             &TCPConnectorClient::TCPConnectorHandleConnection,
                             /*status=*/ 0, ch);
        // reset the connector
        client_ = NULL;
        return;
    }

    // failed to connect to the specified server
    ASSERT(events & EPOLLERR);
    ERROR(log_) << "Failed to connect. fd=" << fd;

    // notify error and reset the connector
    ThreadPool::Schedule(client_, &client_t::TCPConnectorHandleConnection,
                         /*status=*/ -1, (TCPChannel *) NULL);
    client_ = NULL;
}
