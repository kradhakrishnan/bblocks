#include <iostream>
#include <boost/pointer_cast.hpp>
#include <sys/time.h>

#include "core/test/unit-test.h"
#include "core/util.hpp"
#include "core/tcpserver.h"

using namespace std;
using namespace dh_core;

uint64_t
NowInMilliSec()
{
    timeval tv;
    int status = gettimeofday(&tv, /*tz=*/ NULL);
    ASSERT(!status);
    return tv.tv_sec * 1000 + (tv.tv_usec / 1000);
}

class BasicTCPTest : public TCPServerClient, public TCPChannelClient
{
public:

    static const uint32_t MAX_ITERATION = 20;
    static const uint32_t WBUFFERSIZE = 4 * 1024;  // 4 KiB
    static const uint32_t TIMEINTERVAL_MS = 1 * 1000;   // 1 s

    BasicTCPTest()
        : log_("testtcp/")
        , addr_("127.0.0.1", 9999 + (rand() % 100))
        , server_ch_(NULL)
        , client_ch_(NULL)
        , count_(0)
        , iter_(0)
    {
    }

    ~BasicTCPTest()
    {
        // TODO: trash the buffers
    }

    void Start(int nonce)
    {
        epoll_ = new EpollSet("serverEpoll/");
        tcpServer_ = new TCPServer(addr_, epoll_);
        tcpServer_->Accept(this, make_cb(this, &BasicTCPTest::AcceptStarted));
    }

    void AcceptStarted(int status)
    {
        ASSERT(status == 0);

        tcpClient_ = new TCPConnector(epoll_);
        ThreadPool::Schedule(tcpClient_, &TCPConnector::Connect, addr_,
                             make_cb(this, &BasicTCPTest::HandleClientConn));
    }

    virtual void TCPServerHandleConnection(int status, TCPChannel * ch)
    {
        ASSERT(status == 0);

        INFO(log_) << "Accepted.";

        server_ch_ = ch;
        server_ch_->InitChannel(this);
    }

    virtual void HandleClientConn(int status, TCPChannel * ch)
    {
        ASSERT(status == 0);

        INFO(log_) << "Connected.";

        client_ch_ = ch;
        client_ch_->InitChannel(this);

        SendData();
    }

    virtual void TCPChannelClientHandleRead(int status, DataBuffer * buf)
    {
        ASSERT(status == 0);

        ThreadPool::Schedule(this, &BasicTCPTest::VerifyData, buf);
    }

    void ClientWriteDone(int status)
    {
        ASSERT(status == 0);

        INFO(log_) << "ClientWriteDone.";

        SendData();
    }

    void VerifyData(DataBuffer *buf)
    {
        for (DataBuffer::iterator it = buf->Begin(); it != buf->End();) {
            ASSERT(!cksum_.empty());
            ASSERT(it->Size());

            // DEBUG(log_) << it->Dump();

            const uint32_t peeksize = WBUFFERSIZE - count_;
            const uint32_t size = std::min(peeksize, it->Size());
            adler32_.Update(it->Get(), size);

            if (it->Size() <= peeksize) {
                count_ += it->Size();
                ++it;
            } else {
                it->Cut(peeksize);
                count_ += peeksize;
            }

            if (count_ >= WBUFFERSIZE) {
                ASSERT(count_ == WBUFFERSIZE);
                ASSERT(cksum_.front() == adler32_.Hash());
                count_ = 0;
                cksum_.pop_front();
                adler32_.Reset();

                DEBUG(log_) << "POP NEXT:" << (int) cksum_.front()
                            << " EMPTY:" << cksum_.empty();
           }
        }

        if (cksum_.empty() && iter_ > MAX_ITERATION) {
            ASSERT(!"Stop");
            // we have reached our sending limit
            return;
        }


        delete buf;
    }

    int MBps(uint64_t bytes, uint64_t ms)
    {
        return (bytes / (1024 * 1024)) / (ms / 1000);
    }

    void SendData()
    {
        if (iter_ > MAX_ITERATION) {
            // we have reached our sending limit
            return;
        }

        INFO(log_) << "SendData.";

        DataBuffer * buf = new DataBuffer();
        RawData data(WBUFFERSIZE);
        data.FillRandom();
        buf->Append(data);

        const uint32_t cksum = Adler32::Calc(data);
        DEBUG(log_) << "PUSH " << (uint32_t) cksum;
        cksum_.push_back(cksum);

        client_ch_->Write(buf, make_cb(this, &BasicTCPTest::ClientWriteDone));

        ++iter_;
    }

    LogPath log_;
    SocketAddress addr_;
    EpollSet * epoll_;
    TCPServer * tcpServer_;
    TCPConnector * tcpClient_;
    TCPChannel * server_ch_;
    TCPChannel * client_ch_;
    list<uint32_t> cksum_;
    uint32_t count_;
    uint32_t iter_;
    Adler32 adler32_;
};

void
test_tcp_basic()
{
    ThreadPool::Start(/*ncores=*/ 4);

    BasicTCPTest test;
    ThreadPool::Schedule(&test, &BasicTCPTest::Start, /*nonce=*/ 0);

    ThreadPool::Wait();
}

int
main(int argc, char ** argv)
{
    srand(time(NULL));

    InitTestSetup();

    TEST(test_tcp_basic);

    TeardownTestSetup();

    return 0;
}
