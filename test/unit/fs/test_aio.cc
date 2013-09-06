#include <iostream>
#include <boost/pointer_cast.hpp>
#include <sys/time.h>

#include "test/unit-test.h"
#include "util.hpp"
#include "fs/aio-linux.h"
#include "async.h"

using namespace std;
using namespace dh_core;

//............................................................ basicaiotest ....

class BasicAioTest : public CompletionHandle
{
public:

    typedef BasicAioTest This;

    static const uint32_t MAX_ITERATION = 20;
    static const uint32_t WBUFFERSIZE = 4 * 1024; // 4k
    static const uint32_t TIMEINTERVAL_MS = 1 * 1000;   // 1 s

    struct IOCtx
    {
        IOCtx(const int off, const IOBuffer & buf) : off_(off), buf_(buf) {}

        int off_;
        IOBuffer buf_;
    };

    //.... create/destroy ....//

    BasicAioTest()
        : log_("testaio/")
        , aio_(new LinuxAioProcessor())
        , iter_(0)
        , dev_("../build/test.out", /*size=*/ 10 * 1024 * 1024, aio_)
    {
    }

    ~BasicAioTest()
    {
        // TODO: trash the buffers
    }

    void Start(int nonce)
    {
        int status = dev_.OpenDevice();
        INVARIANT(status > 0);

        // fill up the buffer

        // start writing data to device
        for (int i = 0; i < 1000; ++i) {
            count_.Add(/*val=*/ 1);

            IOBuffer buf = IOBuffer::Alloc(WBUFFERSIZE);
            IOCtx * ctx = new IOCtx(i, buf);
            buf.Fill('a' + (i % 26));
            const int status = dev_.Write(ctx->buf_, (i * WBUFFERSIZE) / 512,
                                          WBUFFERSIZE / 512,
                                          async_fn(this, &BasicAioTest::WriteDone,
                                                   ctx));
            INVARIANT(status == 1);
        }
    }

    //.... handlers ....//

    __completion_handler__
    void WriteDone(int status, IOCtx * ctx)
    {
        ASSERT(ctx);
        ASSERT(status != -1 && size_t(status) == ctx->buf_.Size());

        DEBUG(log_) << "WriteDone(" << status << ", " << ctx->off_ << ")";

        ctx->buf_.Fill(/*ch=*/ 0);
        dev_.Read(ctx->buf_, (ctx->off_ * WBUFFERSIZE) / 512, WBUFFERSIZE / 512,
                  async_fn(this, &BasicAioTest::ReadDone, ctx));
    }

    __completion_handler__
    void ReadDone(int status, IOCtx * ctx)
    {
        ASSERT(ctx);
        ASSERT(status != -1 && size_t(status) == ctx->buf_.Size());

        DEBUG(log_) << "Read done(" << status << ", " << ctx->off_ << ")";

        for (size_t i = 0; i < ctx->buf_.Size(); ++i) {
            ASSERT(ctx->buf_.Ptr()[i] == uint8_t('a' + (ctx->off_ % 26)));
        }

        ctx->buf_.Trash();
        delete ctx;

        // TODO: Unregister and then shutdown

        if (count_.Add(/*val=*/ -1) == 1) {
            ThreadPool::Shutdown();
        }
    }

private: 

    LogPath log_;
    AioProcessor * aio_;
    uint32_t iter_;
    SpinningDevice dev_;
    AtomicCounter count_;
};

void
test_aio_basic()
{
    ThreadPool::Start(/*ncores=*/ 4);

    BasicAioTest test;

    ThreadPool::Schedule(&test, &BasicAioTest::Start, /*nonce=*/ 0);

    ThreadPool::Wait();
}

//.................................................................... main ....

int
main(int argc, char ** argv)
{
    srand(time(NULL));

    InitTestSetup();

    TEST(test_aio_basic);

    TeardownTestSetup();

    return 0;
}