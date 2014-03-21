#include <boost/program_options.hpp>
#include <string>
#include <iostream>
#include <memory>

#include "fs/aio-linux.h"
#include "test/unit/unit-test.h"

using namespace std;
using namespace dh_core;

namespace po = boost::program_options;

size_t _time_s = 10; // 10s

struct Stats
{
    Stats()
        : bytes_(0), ops_time_ms_(0), ops_count_(0)
    {}

    Timer ms_;
    uint64_t bytes_;
    uint64_t ops_time_ms_;
    uint64_t ops_count_;
};

//............................................................ AIOBenchamrk ....

/**
 */
class AIOBenchmark : public CompletionHandle
{
public:

    typedef AIOBenchmark This;

    enum IOType
    {
        READ = 0,
        WRITE,
    };

    enum IOPattern
    {
        SEQUENTIAL = 0,
        RANDOM,
    };

    //.... create/destroy ....//

    AIOBenchmark(const string & devname, const disksize_t devsize,
                 const size_t iosize, const IOType iotype,
                 const IOPattern iopattern, const size_t qdepth)
        : log_("/aiobmark")
        , lock_("/aiobmark")
        , devname_(devname)
        , devsize_(devsize)
        , iosize_(iosize)
        , iotype_(iotype)
        , iopattern_(iopattern)
        , qdepth_(qdepth)
        , aio_(new LinuxAioProcessor())
        , dev_(devname_, devsize_ / 512, aio_)
        , buf_(IOBuffer::Alloc(iosize_))
        , nextOff_(UINT64_MAX) 
    {
        INVARIANT(!(iosize_ % 512));
        INVARIANT(!(devsize_ % 512));
    }

    virtual ~AIOBenchmark() {}

    diskoff_t NextOff()
    {
        AutoLock _(&lock_);

        if (nextOff_ == UINT64_MAX) {
            nextOff_ = 0;
            return nextOff_;
        }

        if (iopattern_ == SEQUENTIAL) {
            nextOff_ += iosize_;
            if ((nextOff_ + iosize_) >= devsize_) {
                nextOff_ = 0;
            }
        } else {
            nextOff_ = rand() % (devsize_  - iosize_);
        }

        return nextOff_;
    }

    void Start(int)
    {
        int status = dev_.OpenDevice();
        INVARIANT(status != -1);

        for (size_t i = 0; i < qdepth_; ++i) {
            IssueNextIO();
        }
    }

    //.... handlers ....//

    __completion_handler__
    void WriteDone(int status, diskoff_t off)
    {
        DEBUG(log_) << "Write complete for " << off;

        INVARIANT(status > 0 && size_t(status) == buf_.Size());

        {
            AutoLock _(&lock_);
            stats_.bytes_ += status;
        }

        pendingOps_.Add(/*val=*/ -1);

        if (stats_.ms_.Elapsed() < (_time_s * 1000)) {
            IssueNextIO();
        } else if (!pendingOps_.Count()) {
            Stop();
        }
    }

    __completion_handler__
    void ReadDone(int status, diskoff_t off)
    {
        DEBUG(log_) << "Read complete for " << off;

        INVARIANT(status > 0 && size_t(status) == buf_.Size());

        {
            AutoLock _(&lock_);
            stats_.bytes_ += status;
        }

        pendingOps_.Add(/*val=*/ -1);

        if (stats_.ms_.Elapsed() < (_time_s * 1000)) {
            IssueNextIO();
        } else if (!pendingOps_.Count()) {
            Stop();
        }
    }

private:

    void IssueNextIO()
    {
        pendingOps_.Add(/*val=*/ 1);

        if (iotype_ == READ) {
            diskoff_t off = NextOff();
            dev_.Read(buf_, off / 512, iosize_ / 512,
                      intr_fn(this, &AIOBenchmark::ReadDone, off));
        } else {
            diskoff_t off = NextOff();
            dev_.Write(buf_, off / 512, iosize_ / 512,
                       intr_fn(this, &AIOBenchmark::WriteDone, off));
        }
    }

    void Stop()
    {
        PrintStats();
        ThreadPool::Shutdown();
    }

    void PrintStats()
    {
        AutoLock _(&lock_);

        double MiB = stats_.bytes_ / (1024 * 1024);
        double s = stats_.ms_.Elapsed() / 1000;

        cout << "Result :" << endl
             << " bytes " << stats_.bytes_ << endl
             << " time " << stats_.ms_.Elapsed() << " ms" << endl
             << " MBps " << (s ? (MiB / s) : 0) << endl;
    }

    LogPath log_;
    SpinMutex lock_;
    const string devname_;
    const disksize_t devsize_;
    const size_t iosize_;
    const IOType iotype_;
    const IOPattern iopattern_;
    const size_t qdepth_;
    AioProcessor * aio_;
    SpinningDevice dev_;
    IOBuffer buf_;
    diskoff_t nextOff_;
    Stats stats_;
    AtomicCounter pendingOps_;
};

//.................................................................... Main ....

int
main(int argc, char ** argv)
{
    string devname;
    disksize_t devsize;
    size_t iosize;
    string iotype;
    string iopattern;
    size_t qdepth;
    size_t ncpu;

    po::options_description desc("Options:");
    desc.add_options()
        ("help", "Print usage")
        ("devpath", po::value<string>(&devname)->required(), "Device path")
        ("devsize", po::value<disksize_t>(&devsize)->required(), 
         "Device size in GiB")
        ("iosize", po::value<size_t>(&iosize)->required(), "io size in B")
        ("iotype", po::value<string>(&iotype)->required(), "read/write")
        ("iopattern", po::value<string>(&iopattern)->required(), "seq/random")
        ("qdepth", po::value<size_t>(&qdepth)->required(), "Queue depth")
        ("ncpu", po::value<size_t>(&ncpu)->required(), "Number of cores")
        ("s", po::value<size_t>(&_time_s), "Test time in s");

    po::variables_map parg;

    try {
        po::store(po::parse_command_line(argc, argv, desc), parg);
        po::notify(parg);
    } catch(...) {
        cerr << "Both -c and -s are provided." << endl;
        cout << desc << endl;
        return -1;
    }

    // help command
    if (parg.count("help")) {
        cout << desc << endl;
        return 0;
    }

    // invalid commands
    if ((iotype != "read" && iotype != "write")
        || (iopattern != "random" && iopattern != "seq")) {
        cerr << "unknown iotype or iopattern" << endl;
        cerr << desc << endl;
        return -1;
    }

    // run benchamark
    InitTestSetup();
    ThreadPool::Start(ncpu);

    cout << "Running benchmark for"
         << " devname " << devname << endl
         << " devsize " << devsize << " Gib" << endl
         << " iosize " << iosize << endl
         << " iotype " << iotype << endl
         << " iopattern " << iopattern << endl
         << " qdepth " << qdepth << endl;

    AIOBenchmark bmark(devname, devsize * 1024 * 1024 * 1024, iosize,
                       iotype == "read" ? AIOBenchmark::READ
                                        : AIOBenchmark::WRITE,
                       iopattern == "random" ? AIOBenchmark::RANDOM
                                             : AIOBenchmark::SEQUENTIAL, qdepth);

    ThreadPool::Schedule(&bmark, &AIOBenchmark::Start, /*status=*/ 0);
    ThreadPool::Wait();

    TeardownTestSetup();
    return 0;
}
