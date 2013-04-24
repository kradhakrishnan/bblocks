#ifndef _TCPSERVER_H_
#define _TCPSERVER_H_

#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <list>

#include "core/util.hpp"
#include "core/epollset.h"
#include "core/thread-pool.h"
#include "core/callback.hpp"

namespace dh_core {

/**
 *
 */
class SocketOptions
{
public:

    static bool SetTcpNoDelay(const int fd, const bool enable)
    {
        int flag = enable;
        int status = setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (char *) &flag,
                                sizeof(int));
        return status != -1;
    }

    static bool SetTcpWindow(const int fd, const int size)
    {
        int status;

        status = setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &size, sizeof(size));
        if (status == -1) return false;

        status = setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &size, sizeof(size));
        return status != -1;
    }
};

/**
 *
 */
class SocketAddress
{

public:

    SocketAddress()
    {
    }

    SocketAddress(const std::string & hostname, const short & port)
    {
        LookupByHost(hostname, port);
    }

    SocketAddress(const short & port)
    {
        SetAddress(INADDR_ANY, port);
    }

    void LookupByHost(const std::string & hostname, const short port)
    {
        addrinfo hints;
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_flags = 0;
        hints.ai_protocol = 0;

        addrinfo * result = NULL;
        int status = getaddrinfo(hostname.c_str(), NULL, &hints, &result);
        ASSERT(status != -1);
        ASSERT(result);
        ASSERT(result->ai_addrlen == sizeof(sockaddr_in));

        // sockaddr_in ret = *((sockaddr_in *)result->ai_addr);
        memset(&addr_, 0, sizeof(sockaddr_in)); 
        addr_.sin_family = AF_INET;
        addr_.sin_port = htons(port);
        addr_.sin_addr.s_addr = ((sockaddr_in *)result->ai_addr)->sin_addr.s_addr;

        freeaddrinfo(result);
    }

    const sockaddr_in & sockaddr() const
    {
        return addr_;
    }

private:

    void SetAddress(const in_addr_t & addr, const short port)
    {
        memset(&addr_, 0, sizeof(sockaddr_in));
        addr_.sin_family = AF_INET;
        addr_.sin_port = htons(port);
        addr_.sin_addr.s_addr = addr;
    }

    struct sockaddr_in addr_;
};

/**
 *
 */
class TCPConnector;
class TCPServer;

class TCPChannel : public EpollSetClient
{
public:

    friend class TCPConnector;
    friend class TCPServer;

    typedef Callback<int> writercb_t;
    typedef Callback2<int, DataBuffer *> readercb_t;

    TCPChannel(const std::string & name, int fd, EpollSet * epoll)
        : log_(name)
        , fd_(fd)
        , epoll_(epoll)
        , reader_cb_(NULL)
        , writer_cb_(NULL)
    {
        ASSERT(fd_ >= 0);
        ASSERT(epoll_);
    }

    ~TCPChannel()
    {
        ASSERT(!reader_cb_);
        ASSERT(!writer_cb_);
    }

    void Write(const DataBuffer & buf, writercb_t * cb);

    // TODO: Make reader subscribe/unsubscribe model
    void RegisterRead(readercb_t * cb);

    // TODO: Make this asynchronous, that is the correct protocol
    void Close();

    void EpollSetHandleFdEvent(int fd, uint32_t events);

private:

    static const uint32_t DEFAULT_READ_SIZE = 640 * 1024; // 640K

    TCPChannel();

    // Read data from the socket to internal buffer
    void ReadDataFromSocket();
    // Write data from internal buffer to socket
    void WriteDataToSocket();

    // close socket
    void CloseInternal();

    LogPath log_;
    int fd_;
    EpollSet * epoll_;
    DataBuffer inq_;
    DataBuffer outq_;
    readercb_t * reader_cb_;
    writercb_t * writer_cb_;
};

/**
 *
 */
class TCPServerClient
{
public:

    virtual void TCPServerHandleConnection(int status, TCPChannel * ch) = 0;
};

/**
 * TCPServer
 */
class TCPServer : public EpollSetClient
{
public:

    TCPServer(const SocketAddress & addr, EpollSet * epoll)
        : log_(GetLogPath())
        , epoll_(epoll)
        , servaddr_(addr)
        , client_(NULL)
    {
        ASSERT(epoll_);
        ASSERT(!client_);
    }

    ~TCPServer()
    {
        INVARIANT(!client_);
        INVARIANT(!epoll_);
    }

    uint64_t UId() const
    {
        return (uint64_t) this;
    }

    void StartAccepting(TCPServerClient * client);
    // TODO: Make it async 
    void Shutdown();

private:

    void EpollSetHandleFdEvent(int fd, uint32_t events);

    const std::string GetLogPath() const
    {
        return "tcpserver/" + STR((uint64_t) this);
    }

    const std::string TCPChannelLogPath(int fd)
    {
        return "tcpserver/" + STR(sockfd_) + "/ch/" + STR(fd) + "/";
    }

    static const unsigned int MAXBACKLOG = 100;

    typedef int socket_t;
    typedef std::list<TCPChannel *> iochs_t;

    LogPath log_;
    EpollSet * epoll_;
    socket_t sockfd_;
    const SocketAddress servaddr_;
    iochs_t iochs_;
    TCPServerClient * client_;
};

/**
 *
 */
class TCPConnectorClient
{
public:

    virtual void TCPConnectorHandleConnection(int status, TCPChannel * ch) = 0;
};

/**
 * TCPClient
 */
class TCPConnector : public EpollSetClient
{

public:

    typedef TCPConnectorClient client_t;

    TCPConnector(EpollSet * epoll, const std::string & name = "tcpclient/")
        : log_(name)
        , epoll_(epoll)
        , client_(NULL)
    {
    }

    ~TCPConnector()
    {
        ASSERT(!client_);
    }

    void Connect(const SocketAddress & addr, TCPConnectorClient *cb);
    void Stop();

private:

    void EpollSetHandleFdEvent(int fd, uint32_t events);

    std::string TCPChannelLogPath(int fd) const
    {
        return  log_.GetPath() + "ch/" + STR(fd) + "/";
    }

    LogPath log_;
    EpollSet * epoll_;
    TCPConnectorClient * client_;
};

} // namespace kware {

#endif
