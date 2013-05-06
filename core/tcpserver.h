#ifndef _TCPSERVER_H_
#define _TCPSERVER_H_

#include <boost/algorithm/string.hpp>
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
#include "core/buffer.h"

namespace dh_core {

//................................................................. Helpers ....

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

    /*.... Static Function ....*/

    static sockaddr_in GetAddr(const std::string & hostname, const short port)
    {
        sockaddr_in addr;

        addrinfo hints;
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_flags = 0;
        hints.ai_protocol = 0;

        addrinfo * result = NULL;
        int status = getaddrinfo(hostname.c_str(), NULL, &hints, &result);
        INVARIANT(status != -1);
        ASSERT(result);
        ASSERT(result->ai_addrlen == sizeof(sockaddr_in));

        // sockaddr_in ret = *((sockaddr_in *)result->ai_addr);
        memset(&addr, 0, sizeof(sockaddr_in)); 
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        addr.sin_addr.s_addr = ((sockaddr_in *)result->ai_addr)->sin_addr.s_addr;

        freeaddrinfo(result);

        return addr;
    }

    static sockaddr_in GetAddr(const in_addr_t & addr, const short port)
    {
        sockaddr_in sockaddr;
        memset(&sockaddr, /*ch=*/ 0, sizeof(sockaddr_in));
        sockaddr.sin_family = AF_INET;
        sockaddr.sin_port = htons(port);
        sockaddr.sin_addr.s_addr = addr;

        return sockaddr;
    }

    static sockaddr_in GetAddr(const std::string & saddr)
    {
        std::vector<std::string> tokens;
        boost::split(tokens, saddr, boost::is_any_of(":"));

        ASSERT(tokens.size() == 2);

        const std::string host = tokens[0];
        const short port = atoi(tokens[1].c_str());

        return GetAddr(host, port);
    }

    static SocketAddress GetAddr(const std::string & laddr,
                                 const std::string & raddr)
    {
        return SocketAddress(GetAddr(laddr), GetAddr(raddr));
    }

    /*.... ctor/dtor ....*/

    SocketAddress()
    {
    }

    SocketAddress(const sockaddr_in & raddr)
        : laddr_(GetAddr(INADDR_ANY, /*port=*/ 0)), raddr_(raddr)
    {}

    SocketAddress(const sockaddr_in & laddr, const sockaddr_in & raddr)
        : laddr_(laddr), raddr_(raddr)
    {}

    /*.... get/set ....*/

    const sockaddr_in & LocalAddr() const
    {
        return laddr_;
    }

    const sockaddr_in & RemoteAddr() const
    {
        return raddr_;
    }

private:

    struct sockaddr_in laddr_;
    struct sockaddr_in raddr_;
};

class TCPConnector;
class TCPServer;
class TCPChannel;

//.............................................................. TCPChannel ....

/**
 *
 */
class TCPChannelClient
{
public:

    virtual ~TCPChannelClient() {}

    /**
     *
     */
    __async__ virtual void TcpReadDone(TCPChannel * ch, int status,
                                       IOBuffer buf) = 0;

    /**
     *
     */
    __async__ virtual void TcpWriteDone(TCPChannel * ch, int status) = 0;
};

/**
 *
 */
class TCPChannel : public EpollSetClient
{
public:

    friend class TCPConnector;
    friend class TCPServer;

    //.... statics ....//

    static const uint32_t DEFAULT_WRITE_BACKLOG = IOV_MAX; 

    //.... construction/destruction ....//

    TCPChannel(const std::string & name, int fd, EpollSet * epoll)
        : log_(name)
        , fd_(fd)
        , epoll_(epoll)
        , client_(NULL)
        , wbuf_(DEFAULT_WRITE_BACKLOG)
    {
        ASSERT(fd_ >= 0);
        ASSERT(epoll_);
    }

    virtual ~TCPChannel()
    {
        ASSERT(client_);
    }

    //.... member fns ....//

    /**
     *
     */
    bool EnqueueWrite(const IOBuffer & data);

    /**
     *
     */
    void Read(IOBuffer & data);

    /**
     *
     */
    void RegisterClient(TCPChannelClient * client_);

    /**
     *
     */
    void UnregisterClient(TCPChannelClient * client_, Callback<int> * cb);

    /**
     *
     */
    void Close();

    /**
     *
     */
    void EpollSetHandleFdEvent(int fd, uint32_t events);

private:

    struct ReadCtx
    {
        ReadCtx() : bytesRead_(0) {}
        ReadCtx(const IOBuffer & buf) : buf_(buf), bytesRead_(0) {}

        void Reset()
        {
            buf_.Reset();
            bytesRead_ = 0;
        }

        IOBuffer buf_;
        uint32_t bytesRead_;
    };

    TCPChannel();

    // Read data from the socket to internal buffer
    void ReadDataFromSocket();
    // Write data from internal buffer to socket
    void WriteDataToSocket();

    SpinMutex lock_;
    LogPath log_;
    int fd_;
    EpollSet * epoll_;
    TCPChannelClient * client_;
    BoundedQ<IOBuffer> wbuf_;
    ReadCtx rctx_;
};

//............................................................... TCPServer ....

/**
 *
 */
class TCPServerClient
{
public:

    virtual void TCPServerHandleConnection(int status, TCPChannel * ch) = 0;
    virtual ~TCPServerClient() {}
};

/**
 * TCPServer
 */
class TCPServer : public EpollSetClient
{
public:

    TCPServer(const sockaddr_in & addr, EpollSet * epoll)
        : log_(GetLogPath())
        , epoll_(epoll)
        , servaddr_(addr)
        , client_(NULL)
    {
        ASSERT(epoll_);
        ASSERT(!client_);
    }

    virtual ~TCPServer()
    {
        INVARIANT(!client_);
        INVARIANT(!epoll_);
    }

    uint64_t UId() const
    {
        return (uint64_t) this;
    }

    void Accept(TCPServerClient * client, Callback<status_t> * cb = NULL);
    // TODO: Make it async 
    void Shutdown(Callback<int> * cb = NULL);
    void ShutdownFdRemoved(int status, Callback<int> * cb); 

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
    const sockaddr_in servaddr_;
    iochs_t iochs_;
    TCPServerClient * client_;
};

//............................................................ TCPConnector ....

/**
 * TCPClient
 */
class TCPConnector : public EpollSetClient
{

public:

    TCPConnector(EpollSet * epoll, const std::string & name = "tcpclient/")
        : log_(name)
        , epoll_(epoll)
    {
    }

    virtual ~TCPConnector()
    {
        ASSERT(clients_.empty());
    }

    void Connect(const SocketAddress addr, Callback2<int, TCPChannel *> * cb);

    void Stop(Callback<int> * cb = NULL);

private:

    void EpollSetHandleFdEvent(int fd, uint32_t events);
    void EpollSetAddDone(const int status);

    typedef std::map<fd_t, Callback2<int, TCPChannel *> *> clients_map_t;

    std::string TCPChannelLogPath(int fd) const
    {
        return  log_.GetPath() + "ch/" + STR(fd) + "/";
    }

    SpinMutex lock_;
    LogPath log_;
    EpollSet * epoll_;
    clients_map_t clients_;
};

} // namespace kware {

#endif
