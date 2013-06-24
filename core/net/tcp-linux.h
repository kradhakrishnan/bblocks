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
#include "core/thread-pool.h"
#include "core/callback.hpp"
#include "core/buffer.h"
#include "core/async.h"

#include "core/net/epoll.h"

namespace dh_core {

class TCPChannel;
class TCPServer;
class TCPConnector;

//........................................................... SocketOptions ....

///
/// @class SocketOptions
///
/// Abstraction to manipulate socket options.
///
///
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

// .......................................................... SocketAddress ....

///
/// @class SocketAddress
///
/// Socket address abstraction.
///
///
class SocketAddress
{

public:

    /*.... Static Function ....*/

    ///
    /// Convert hostname:port to sockaddr_in
    ///
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

        memset(&addr, 0, sizeof(sockaddr_in)); 
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        addr.sin_addr.s_addr = ((sockaddr_in *)result->ai_addr)->sin_addr.s_addr;

        freeaddrinfo(result);

        return addr;
    }

    ///
    /// Convert IP:port to sockaddr_in
    ///
    static sockaddr_in GetAddr(const in_addr_t & addr, const short port)
    {
        sockaddr_in sockaddr;
        memset(&sockaddr, /*ch=*/ 0, sizeof(sockaddr_in));
        sockaddr.sin_family = AF_INET;
        sockaddr.sin_port = htons(port);
        sockaddr.sin_addr.s_addr = addr;

        return sockaddr;
    }

    ///
    /// Convert hostname:port string to sockaddr_in
    ///
    static sockaddr_in GetAddr(const std::string & saddr)
    {
        std::vector<std::string> tokens;
        boost::split(tokens, saddr, boost::is_any_of(":"));

        ASSERT(tokens.size() == 2);

        const std::string host = tokens[0];
        const short port = atoi(tokens[1].c_str());

        return GetAddr(host, port);
    }

    ///
    /// Construct a connection (local binding-remote binding) form laddr and
    /// raddr strings of format host:port
    ///
    static SocketAddress GetAddr(const std::string & laddr,
                                 const std::string & raddr)
    {
        return SocketAddress(GetAddr(laddr), GetAddr(raddr));
    }

    /*.... ctor/dtor ....*/

    explicit SocketAddress(const sockaddr_in & raddr)
        : laddr_(GetAddr(INADDR_ANY, /*port=*/ 0)), raddr_(raddr)
    {}

    explicit SocketAddress(const sockaddr_in & laddr, const sockaddr_in & raddr)
        : laddr_(laddr), raddr_(raddr)
    {}

    /*.... get/set ....*/

    ///
    /// Get local binding socket address
    ///
    const sockaddr_in & LocalAddr() const { return laddr_; }

    ///
    /// Get remote socket address
    ///
    const sockaddr_in & RemoteAddr() const { return raddr_; }

private:

    struct sockaddr_in laddr_;
    struct sockaddr_in raddr_;
};

//.............................................................. TCPChannel ....

///
/// @class TCPChannel
///
class TCPChannel : public AsyncProcessor, public CompletionHandle
{
public:

    friend class TCPConnector;
    friend class TCPServer;
    friend class Epoll;

    /*.... Callback defintions ....*/

    typedef CHandler3<TCPChannel *, int, IOBuffer> ReadDoneHandler;
    typedef CHandler2<TCPChannel *, int> WriteDoneHandler;
    typedef AsyncProcessor::UnregisterDoneFn UnregisterDoneFn;

    /* .... Create/Destroy .... */

    explicit TCPChannel(const std::string & name, int fd, Epoll & epoll);
    virtual ~TCPChannel();

    /* .... CHandle override .... */

    void RegisterHandle(CHandle * h);
    __async_operation__ void UnregisterHandle(CHandle * h, UnregisterDoneFn cb);

    /* .... async operations .... */

    ///
    ///
    ///
    __async_operation__ int EnqueueWrite(const IOBuffer & data);

    ///
    /// Invoke asynchronous read operation
    ///
    /// Will try to read data from the socket. If not readily available it will
    /// return asynchronous call when read otherwise will read and return
    /// immediately.
    ///
    /// @param   data    Data to be written to the socket
    /// @param   fn      Callback when read (if data not readily available)
    /// @return  true if data read is done else false
    ///
    __async_operation__ bool Read(IOBuffer & data,
                                  const ReadDoneHandler & chandler);


    /* .... sync operations .... */

    ///
    /// Close the channel communication paths
    ///
    void Close();

    ///
    /// Set callback function for write done
    ///
    void SetWriteDoneFn(const WriteDoneHandler & chandler)
    {
        INVARIANT(client_.h_ == chandler.GetHandle());
        client_.writeDoneHandler_ = chandler;
    }

private:

    /* .... Inaccessible .... */

    TCPChannel();

    /*.... Constants ....*/

    static const uint32_t DEFAULT_WRITE_BACKLOG = 1.5 * IOV_MAX; 

    /* .... Data .... */

    ///
    /// Represent client and its callbacks
    ///
    struct Client
    {
        Client() : h_(NULL), unregisterDoneFn_(NULL) {}

        Client(CHandle * h)
            : h_(h)
            , unregisterDoneFn_(NULL)
        {}

        CHandle * h_;
        WriteDoneHandler writeDoneHandler_;
        UnregisterDoneFn unregisterDoneFn_;
    };

    ///
    /// represent read operation context
    ///
    struct ReadCtx
    {
        ReadCtx() : bytesRead_(0) {}

        ReadCtx(const IOBuffer & buf, const ReadDoneHandler & chandler)
            : buf_(buf)
            , bytesRead_(0)
            , chandler_(chandler)
        {}

        void Reset()
        {
            buf_.Reset();
            bytesRead_ = 0;
        }

        IOBuffer buf_;
        uint32_t bytesRead_;
        ReadDoneHandler chandler_;
    };

    /* .... Private member methods .... */

    ///
    /// epoll interrupt for socket notification
    ///
    __interrupt__ void HandleFdEvent(int fd, uint32_t events);

    ///
    /// Read data from the socket to internal buffer
    ///
    bool ReadDataFromSocket(const bool isasync);

    ///
    /// Write data from internal buffer to socket
    ///
    size_t WriteDataToSocket(const bool isasync);

    ///
    /// Notification back from thread pool about drained events
    ///
    void BarrierDone(int);

    /* .... Private member variables .... */

    SpinMutex lock_;
    LogPath log_;
    int fd_;
    Epoll & epoll_;
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

    TCPServer(Epoll * epoll)
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

    __interrupt__ void HandleFdEvent(int fd, uint32_t events);

    const std::string GetLogPath() const
    { return "tcpserver/" + STR((uint64_t) this); }

    const std::string TCPChannelLogPath(int fd)
    { return "tcpserver/" + STR(sockfd_) + "/ch/" + STR(fd) + "/"; }

    //.... private member variables ....//

    SpinMutex lock_;
    LogPath log_;
    Epoll * epoll_;
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

    TCPConnector(Epoll * epoll, const std::string & name = "tcpclient/")
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

    __interrupt__ void HandleFdEvent(int fd, uint32_t events);

    const std::string TCPChannelLogPath(const int fd) const
    { return  log_.GetPath() + "ch/" + STR(fd) + "/"; }

    //.... private member variables ....//

    SpinMutex lock_;
    LogPath log_;
    Epoll * epoll_;
    clients_map_t clients_;
};

} // namespace kware {

#endif
