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

namespace kware {

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

class TCPClient;
class TCPServer;

class TCPChannel : public NonBlockingLogic
{
public:

    friend class TCPClient;
    friend class TCPServer;

    typedef Raw<GenericFn1<bool> > writecb_t;
    typedef Raw<GenericFn> cb_t;

/*
    TCPChannel()
        : Actor(GetLogPath())
        , log_(GetLogPath())
        , fd_(0)
    {
    }
*/
    TCPChannel(int fd, const Ptr<EpollSet> & epoll,
               const std::string & name)
        : Actor(name)
        , log_(name)
        , fd_(fd)
        , epoll_(epoll)
        , reader_(NULL)
        , writer_(NULL)
    {
        ASSERT(fd_ >= 0);
        ASSERT(epoll_);
    }

    ~TCPChannel()
    {
    }

    void Write(const DataBuffer & buf, const writecb_t & cb);
    action_t RegisterRead(const cb_t & cb);
    action_t Close(const cb_t & cb);

private:

    static const uint32_t DEFAULT_READ_SIZE = 640 * 1024; // 640K

    TCPChannel();

    // Handler functions
    void WriteImpl(Actor * writer, const DataBuffer & data);
    void RegisterReadImpl(Actor * reader);
    void CloseImpl(Actor * actor);

    void HandleEpollNotification(const epoll_event & event);
    void ReadDataFromSocket();
    void WriteDataToSocket();

    // close
    void CloseInternal();
    void CloseInternal_EpollClosed();

    LogStream log_;
    int fd_;
    Ptr<EpollSet> epoll_;
    DataBuffer inq_;
    DataBuffer outq_;
    Actor * reader_;
    Actor * writer_;
};

/**
 * TCPServer
 */
class TCPServer : public Actor
{
public:

    TCPServer(const SocketAddress & addr, const Ptr<EpollSet> & epoll)
        : Actor(GetLogPath())
        , log_(GetLogPath())
        , epoll_(epoll)
        , servaddr_(addr)
    {
        ASSERT(epoll_);
    }

    ~TCPServer()
    {
    }

    virtual bool HandleAction(Actor * from, const ActorCommand & type,
                              void * data, void * ctx);

    action_t StartAccepting(Actor * actor);
    action_t Shutdown();

private:

    void HandleEpollNotification(const epoll_event & ee);
    void StartAcceptingImpl(Actor * actor);
    void ShutdownImpl();

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

    LogStream log_;
    Ptr<EpollSet> epoll_;
    socket_t sockfd_;
    const SocketAddress servaddr_;
    iochs_t iochs_;
    Actor * acceptor_;
};

/**
 * TCPClient
 */
class TCPClient : public Actor
{

public:

    TCPClient(const Ptr<EpollSet> & epoll,
              const std::string & name = "tcpclient/")
        : Actor(name)
        , log_(name)
        , epoll_(epoll)
        , connector_(NULL)
    {
    }

    ~TCPClient()
    {
        ASSERT(!connector_);
    }

    virtual bool HandleAction(Actor * from, const ActorCommand & type,
                              void * data, void * ctx);

    action_t Connect(Actor * caller, const SocketAddress & addr);
    void Stop();

private:

    void HandleEpollNotification(const epoll_event & ee);
    void ConnectImpl(Actor * connector, const SocketAddress & addr);

    std::string TCPChannelLogPath(int fd) const
    {
        return name_ + "ch/" + STR(fd) + "/";
    }

    LogStream log_;
    Ptr<EpollSet> epoll_;
    Actor * connector_;
};

} // namespace kware {

#endif
