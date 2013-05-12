#ifndef _TCPSERVER_H_
#define _TCPSERVER_H_

#include <boost/algorithm/string.hpp>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <fcntl.h>

#include <netdb.h>
#include <list>

#include "core/util.hpp"
#include "core/epollset.h"
#include "core/thread-pool.h"
#include "core/callback.hpp"
#include "core/buffer.h"
#include "core/async.h"

namespace dh_core {

class TCPChannel;
class TCPServer;
class TCPConnector;

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

    /*! get local binding socket address */
    const sockaddr_in & LocalAddr() const { return laddr_; }
    /*! get remote socket address */
    const sockaddr_in & RemoteAddr() const { return raddr_; }

private:

    struct sockaddr_in laddr_;
    struct sockaddr_in raddr_;
};

//.............................................................. TCPChannel ....

/**
 *
 */
class TCPChannel : public AsyncProcessor, public CompletionHandle
{
public:

    friend class TCPConnector;
    friend class TCPServer;
    friend class EpollSet;

    //.... callback defintions ....//

    typedef void (CHandle::*ReadDoneFn)(TCPChannel *, int, IOBuffer);
    typedef void (CHandle::*WriteDoneFn)(TCPChannel *, int);
    typedef AsyncProcessor::UnregisterDoneFn UnregisterDoneFn;

    //.... statics ....//

    static const uint32_t DEFAULT_WRITE_BACKLOG = IOV_MAX; 

    //.... construction/destruction ....//

    TCPChannel(const std::string & name, int fd, EpollSet * epoll)
        : log_(name)
        , fd_(fd)
        , epoll_(epoll)
        , wbuf_(DEFAULT_WRITE_BACKLOG)
    {
        ASSERT(fd_ >= 0);
        ASSERT(epoll_);
    }

    virtual ~TCPChannel()
    {
        INVARIANT(!client_.h_);
        INVARIANT(!epoll_);
    }

    //.... CHandle override ....//

    void RegisterHandle(CHandle * h);
    __async_operation__ void UnregisterHandle(CHandle * h, UnregisterDoneFn cb);

    //.... async operations ....//

    /**
     *
     */
    __async_operation__ bool EnqueueWrite(const IOBuffer & data);

    /*!
     * \brief Invoke asynchronous read operation
     *
     * Will try to read data from the socket. If not readily available it will
     * return asynchronous call when read otherwise will read and return
     * immediately.
     *
     * \param   data    Data to be written to the socket
     * \param   fn      Callback when read (if data not readily available)
     * \return  true if data read is done else false
     */
    __async_operation__ void Read(IOBuffer & data, const ReadDoneFn fn);


    //.... sync operations ....//

    /*! Close the channel communication paths */
    void Close();

    /*! Set callback function for write done */
    void SetWriteDoneFn(CHandle * h, const WriteDoneFn fn)
    {
        INVARIANT(fn);
        INVARIANT(h && client_.h_ == h);

        client_.writeDoneFn_ = fn;
    }

private:

    // Represent client and its callbacks
    struct Client
    {
        Client() : h_(NULL), writeDoneFn_(NULL), unregisterDoneFn_(NULL) {}
        Client(CHandle * h)
            : h_(h), writeDoneFn_(NULL), unregisterDoneFn_(NULL) {}

        CHandle * h_;
        WriteDoneFn writeDoneFn_;
        UnregisterDoneFn unregisterDoneFn_;
    };

    // represent read operation context
    struct ReadCtx
    {
        ReadCtx() : bytesRead_(0) {}
        ReadCtx(const IOBuffer & buf, const ReadDoneFn fn)
            : buf_(buf), bytesRead_(0), fn_(fn) {}

        void Reset()
        {
            buf_.Reset();
            bytesRead_ = 0;
        }

        IOBuffer buf_;
        uint32_t bytesRead_;
        ReadDoneFn fn_;

    };

    //.... inaccessible ....//

    TCPChannel();

    //.... private member fns ....//

    // epoll interrupt for socket notification
    __interrupt__ void HandleFdEvent(EpollSet * epoll, int fd, uint32_t events);
    // Read data from the socket to internal buffer
    void ReadDataFromSocket();
    // Write data from internal buffer to socket
    void WriteDataToSocket();
    // Notification back from thread pool about drained events
    void BarrierDone(int);

    //.... private member variables ....//

    SpinMutex lock_;
    LogPath log_;
    int fd_;
    EpollSet * epoll_;
    Client client_;
    BoundedQ<IOBuffer> wbuf_;
    ReadCtx rctx_;
};

//............................................................... TCPServer ....

/*!
 * \class TCPServer
 * \brief Asynchronous TCP server implementation
 *
 * Provides a TCP listener implementation. Helps accept connections
 * from clients asynchronously. Designed on the acceptor design pattern.
 */
class TCPServer : public CompletionHandle
{
public:

    //.... callback definitions ....//

    typedef void (CHandle::*ConnDoneFn)(TCPServer *, int, TCPChannel *);

    //.... create/destroy ....//

    TCPServer(EpollSet * epoll)
        : log_(GetLogPath())
        , epoll_(epoll)
    {
        ASSERT(epoll_);
        ASSERT(!client_.h_);
    }

    virtual ~TCPServer()
    {
        INVARIANT(!client_.h_);
        INVARIANT(!epoll_);
    }

    //.... member fns ....//

    void Listen(CHandle * h, sockaddr_in saddr, const ConnDoneFn cb);

    void Shutdown();

private:

    static const size_t MAXBACKLOG = 100;

    struct Client
    {
        Client(CHandle * h = NULL, const ConnDoneFn connDoneFn = NULL)
            : h_(h), connDoneFn_(connDoneFn)
        {}

        CHandle * h_;
        ConnDoneFn connDoneFn_;
    };

    typedef int socket_t;

    //.... private member fns ....//

    __interrupt__ void HandleFdEvent(EpollSet *, int fd, uint32_t events);

    const std::string GetLogPath() const
    { return "tcpserver/" + STR((uint64_t) this); }

    const std::string TCPChannelLogPath(int fd)
    { return "tcpserver/" + STR(sockfd_) + "/ch/" + STR(fd) + "/"; }

    //.... private member variables ....//

    SpinMutex lock_;
    LogPath log_;
    EpollSet * epoll_;
    socket_t sockfd_;
    Client client_;
};

//............................................................ TCPConnector ....

/*!
 * \class TCPConnector
 * \brief Asynchronous TCP connection provider
 *
 * Helps establish TCP connections asynchronously. This follows the connector
 * design pattern.
 *
 */
class TCPConnector : public CompletionHandle
{
public:

    //.... callback definition ....//

    typedef AsyncProcessor::UnregisterDoneFn UnregisterDoneFn;
    typedef void (CHandle::*ConnDoneFn)(TCPConnector *, int, TCPChannel *);

    //.... create/destroy ....//

    TCPConnector(EpollSet * epoll, const std::string & name = "tcpclient/")
        : log_(name)
        , epoll_(epoll)
    {
        ASSERT(epoll_);
    }

    virtual ~TCPConnector()
    {
        ASSERT(clients_.empty());
        epoll_ = NULL;
    }

    //.... async operations ....//

    __async_operation__ void Connect(const SocketAddress addr,
                                     CHandle * h, const ConnDoneFn cb);


    //.... sync functions ....//

    void Shutdown();

private:

    struct Client
    {
        Client(CHandle * h = NULL, const ConnDoneFn connDoneFn = NULL)
            : h_(h), connDoneFn_(connDoneFn) {}

        CHandle * h_;
        ConnDoneFn connDoneFn_;
    };

    typedef std::map<fd_t, Client> clients_map_t;

    //.... private member fns ....//

    __interrupt__ void HandleFdEvent(EpollSet *, int fd, uint32_t events);

    const std::string TCPChannelLogPath(const int fd) const
    { return  log_.GetPath() + "ch/" + STR(fd) + "/"; }

    //.... private member variables ....//

    SpinMutex lock_;
    LogPath log_;
    EpollSet * epoll_;
    clients_map_t clients_;
};

} // namespace kware {

#endif
