#include <iostream>
#include <boost/pointer_cast.hpp>
#include <sys/time.h>

#include "core/test/unit-test.h"
#include "core/util.hpp"
#include "core/fs/aio-linux.h"
#include "core/async.h"

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

    //.... create/destroy ....//

    BasicAioTest()
        : log_("testaio/")
        , aio_(new LinuxAioProcessor())
        , iter_(0)
        , wbuf_(IOBuffer::Alloc(WBUFFERSIZE))
        , rbuf_(IOBuffer::Alloc(WBUFFERSIZE))
        , dev_("test.out", /*size=*/ 10 * 1024 * 1024, aio_)
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
        for (int i = 0; i < 100; ++i) {
            int status = dev_.Write(wbuf_, i, WBUFFERSIZE / 512, this,
                                    async_fn(&BasicAioTest::WriteDone));
            INVARIANT(status == 1);
        }
    }

    //.... handlers ....//

    void WriteDone(BlockDevice *, int status)
    {
        ASSERT(status != -1 && size_t(status) == wbuf_.Size());

        DEBUG(log_) << "WriteDone(" << status << ")";
    }

private: 

    LogPath log_;
    AioProcessor * aio_;
    uint32_t iter_;
    IOBuffer wbuf_;
    IOBuffer rbuf_;
    SpinningDevice dev_;
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
