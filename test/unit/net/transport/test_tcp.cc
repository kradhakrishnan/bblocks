#include <iostream>
#include <boost/pointer_cast.hpp>
#include <sys/time.h>

#include "test/unit-test.h"
#include "util.hpp"
#include "net/transport/tcp-linux.h"
#include "net/epoll/mpio-epoll.h"
#include "async.h"

using namespace std;
using namespace dh_core;

//............................................................ basictcptest ....

class BasicTCPTest : public CompletionHandle
{
public:

    typedef BasicTCPTest This;

    static const uint32_t MAX_ITERATION = 20;
    static const uint32_t WBUFFERSIZE = 4 * 1024;  // 4 KiB
    static const uint32_t TIMEINTERVAL_MS = 1 * 1000;   // 1 s

    BasicTCPTest()
        : lock_("/tcptest/")
	, log_("/testtcp/")
        , epoll_("/epoll")
	, mpepoll_(/*nth=*/ 2)
        , tcpServer_(mpepoll_)
        , tcpClient_(epoll_)
        , addr_(SocketAddress::GetAddr("127.0.0.1", 9999 + (rand() % 100)))
        , server_ch_(NULL)
        , client_ch_(NULL)
        , count_(0)
        , iter_(0)
        , rbuf_(IOBuffer::Alloc(WBUFFERSIZE))
    {
    }

    ~BasicTCPTest()
    {
        // TODO: trash the buffers
    }

    void Start(int nonce)
    {
	SocketAddress saddr = SocketAddress::ServerSocketAddr(addr_);

	int status = tcpServer_.Accept(saddr, async_fn(this, &This::HandleServerConn));
        INVARIANT(status == 0);

        status = tcpClient_.Connect(SocketAddress(addr_), async_fn(this, &This::HandleClientConn));
	INVARIANT(status == 0);
    }

    __completion_handler__
    virtual void HandleServerConn(int status, UnicastTransportChannel * uch)
    {
	TCPChannel * ch = dynamic_cast<TCPChannel *>(uch);

        ASSERT(status == 0);

        INFO(log_) << "Accepted.";

        server_ch_ = ch;

	ReadUntilBlocked();
    }

    void ReadUntilBlocked()
    {
	int status = server_ch_->Read(rbuf_, async_fn(this, &This::ReadDone)); 
	INVARIANT(status >= 0 && status <= (int) rbuf_.Size());

        if (status == (int) rbuf_.Size()) {
		ReadDone(status, rbuf_);
	}
    }

    virtual void HandleClientConn(int status, UnicastTransportChannel * ch) __async_fn__
    {
        ASSERT(status == 0);

        INFO(log_) << "Connected.";

        client_ch_ = dynamic_cast<TCPChannel *>(ch);

        SendData();
    }

    __completion_handler__
    virtual void ReadDone(int status, IOBuffer buf)
    {
	INFO(log_) << "ReadDone. status=" << status;

	if (status == -1) {
		if (iter_ > MAX_ITERATION) {
		    return;
		}
	}

        ASSERT((size_t) status == rbuf_.Size());
        ASSERT(buf == rbuf_);

        VerifyData(rbuf_);

	ReadUntilBlocked();
   }

    __completion_handler__
    virtual void WriteDone(int status, IOBuffer buf)
    {
        ASSERT(status == (int) wbuf_.Size());

        INFO(log_) << "WriteDone. status=" << status;

        wbuf_.Trash();
        SendData();
    }

    virtual void ClientStopped(int status) __async_fn__
    {
	INFO(log_) << "ClientStopped";

        delete client_ch_;
        client_ch_ = NULL;

	server_ch_->Stop(async_fn(this, &This::ServerChannelStopped));
    }

    void ServerChannelStopped(int)
    {
	delete server_ch_;
	server_ch_ = NULL;
	
	tcpServer_.Stop(async_fn(this, &This::ServerStopped));
    }

    void ServerStopped(int)
    {
	tcpClient_.Stop(async_fn(this, &This::ConnectorStopped));
    }

    void ConnectorStopped(int)
    {
	ThreadPool::Wakeup();
    }

private: 

    void VerifyData(IOBuffer & buf)
    {
	Guard _(&lock_);

        const uint32_t cksum = Adler32::Calc(buf.Ptr(), buf.Size());

        INVARIANT(cksum_.front() == cksum);
        cksum_.pop_front();

        DEBUG(log_) << "POP NEXT:" << (int) cksum_.front()
                    << " EMPTY:" << cksum_.empty();

        if (cksum_.empty() && iter_ > MAX_ITERATION) {
	    INFO(log_) << "Stopping client";
            int status = client_ch_->Stop(async_fn(this, &This::ClientStopped));
            INVARIANT(status == 0);
	    return;
        }
    }

    int MBps(uint64_t bytes, uint64_t ms)
    {
        return (bytes / (1024 * 1024)) / (ms / 1000);
    }

    void SendData()
    {
	Guard _(&lock_);

        if (iter_ > MAX_ITERATION) {
            // we have reached our sending limit
            return;
        }

        INFO(log_) << "SendData. iter=" << iter_;

        ASSERT(!wbuf_);
        wbuf_ = IOBuffer::Alloc(WBUFFERSIZE);
        wbuf_.FillRandom();

        const uint32_t cksum = Adler32::Calc(wbuf_.Ptr(), wbuf_.Size());
        DEBUG(log_) << "PUSH " << cksum_.size();
        cksum_.push_back(cksum);

        int status = client_ch_->Write(wbuf_, async_fn(this, &This::WriteDone));
	INVARIANT(status >= 0 && status <= (int) wbuf_.Size());
        if (status == (int) wbuf_.Size()) {
            ThreadPool::Schedule(this, &BasicTCPTest::WriteDone, status, wbuf_);
        }

        ++iter_;
    }

    SpinMutex lock_;
    LogPath log_;
    Epoll epoll_;
    MultiPathEpoll mpepoll_;
    TCPServer tcpServer_;
    TCPConnector tcpClient_;
    sockaddr_in addr_;
    TCPChannel * server_ch_;
    TCPChannel * client_ch_;
    list<uint32_t> cksum_;
    uint32_t count_;
    uint32_t iter_;
    Adler32 adler32_;
    IOBuffer wbuf_;
    IOBuffer rbuf_;
};

void
test_tcp_basic()
{
    ThreadPool::Start(/*ncores=*/ 4);

    BasicTCPTest test;

    ThreadPool::Schedule(&test, &BasicTCPTest::Start, /*nonce=*/ 0);

    ThreadPool::Wait();
    ThreadPool::Shutdown();
}

//.................................................................... main ....

int
main(int argc, char ** argv)
{
    srand(time(NULL));

    InitTestSetup();

    TEST(test_tcp_basic);

    TeardownTestSetup();

    return 0;
}
