#include <boost/assert.hpp>
#include <cassert>
#include <boost/program_options.hpp>
#include <string>
#include <iostream>
#include <memory>

#include "fs/aio-linux.h"
#include "test/unit/unit-test.h"

using namespace std;
using namespace bblocks;

namespace po = boost::program_options;

size_t _time_s = 10; // 10s

struct Stats
{
    Stats()
        : bytes_(0), ops_time_ms_(0), ops_count_(0)
    {}

    Timer ms_;
    atomic<uint64_t> bytes_;
    atomic<uint64_t> ops_time_ms_;
    atomic<uint64_t> ops_count_;
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
		, dev_(devname_, devsize_ / 512, aio_.Ptr())
		, buf_(IOBuffer::Alloc(iosize_))
		, nextOff_(UINT64_MAX)
		, pendingOps_(0)
	{
		INVARIANT(!(iosize_ % 512));
		INVARIANT(!(devsize_ % 512));

		buf_.FillRandom();
	}

	virtual ~AIOBenchmark() {}

	void Start(int)
	{
		int status = dev_.OpenDevice();
		INVARIANT(status != -1);

		for (size_t i = 0; i < qdepth_; ++i) {
			IssueNextIO();
		}
	}

	void WriteDone(int status, diskoff_t off) __async_fn__
	{
		DEBUG(log_) << "Write complete for " << off;

		INVARIANT(status > 0 && size_t(status) == buf_.Size());

		stats_.bytes_ += status;

		if (stats_.ms_.Elapsed() < (_time_s * 1000)) {
			IssueNextIO();
			pendingOps_--;
		} else {
		    /*
		     * Time has elapsed, will issue a stop if we are the last IO
		     */
		    pendingOps_--;
		    if (!pendingOps_) Stop();
		}
	}

	void ReadDone(int status, diskoff_t off)
	{
		DEBUG(log_) << "Read complete for " << off;

		INVARIANT(status > 0 && size_t(status) == buf_.Size());

		stats_.bytes_ += status;

		if (stats_.ms_.Elapsed() < (_time_s * 1000)) {
			IssueNextIO();
			pendingOps_--;
		} else {
			pendingOps_--;
			if (!pendingOps_) Stop();
		}
	}

private:

	void IssueNextIO()
	{
		pendingOps_++;

		if (iotype_ == READ) {
			diskoff_t off = NextOff();
			auto fn = intr_fn(this, &AIOBenchmark::ReadDone, off);
			dev_.Read(buf_, off / 512, iosize_ / 512, fn);
		} else {
			diskoff_t off = NextOff();
			auto fn = intr_fn(this, &AIOBenchmark::WriteDone, off);
			dev_.Write(buf_, off / 512, iosize_ / 512, fn);
		}
	}

	diskoff_t NextOff()
	{
		Guard _(&lock_);

		if (nextOff_ == UINT64_MAX) {
			nextOff_ = 0;
			return nextOff_;
		}

		if (iopattern_ == SEQUENTIAL) {
			nextOff_ += iosize_;
			if ((nextOff_ + iosize_) >= devsize_) nextOff_ = 0;
		} else {
			nextOff_ = rand() % (devsize_  - iosize_);
		}

		return nextOff_;
	}

	void Stop()
	{
		PrintStats();
		BBlocks::Wakeup();
	}

	void PrintStats()
	{
		Guard _(&lock_);

		double MiB = stats_.bytes_ / (1024 * 1024);
		double s = stats_.ms_.Elapsed() / 1000;

		cout << "Result :" << endl
		     << " bytes " << stats_.bytes_ << endl
		     << " time " << stats_.ms_.Elapsed() << " ms" << endl
		     << " MBps " << (s ? (MiB / s) : 0) << endl;
	}

	string log_;
	SpinMutex lock_;
	const string devname_;
	const disksize_t devsize_;
	const size_t iosize_;
	const IOType iotype_;
	const IOPattern iopattern_;
	const size_t qdepth_;
	AutoPtr<AioProcessor> aio_;
	SpinningDevice dev_;
	IOBuffer buf_;
	diskoff_t nextOff_;
	Stats stats_;
	atomic<size_t> pendingOps_;
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
	size_t ncpu = SysConf::NumCores();

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
		("ncpu", po::value<size_t>(&ncpu), "Number of cores")
		("s", po::value<size_t>(&_time_s), "Test time in s");

	po::variables_map parg;

	po::store(po::parse_command_line(argc, argv, desc), parg);
	po::notify(parg);

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
	BBlocks::Start(ncpu);

	cout << "Running benchmark for"
	     << " devname " << devname << endl
	     << " devsize " << devsize << " Gib" << endl
	     << " iosize " << iosize << endl
	     << " iotype " << iotype << endl
	     << " iopattern " << iopattern << endl
	     << " qdepth " << qdepth << endl;

	{
	    AIOBenchmark bmark(devname, devsize * 1024 * 1024 * 1024, iosize,
			   iotype == "read" ? AIOBenchmark::READ
					    : AIOBenchmark::WRITE,
			   iopattern == "random" ? AIOBenchmark::RANDOM
						 : AIOBenchmark::SEQUENTIAL, qdepth);

	    BBlocks::Schedule(&bmark, &AIOBenchmark::Start, /*status=*/ 0);
	    BBlocks::Wait();
	}

	BBlocks::Shutdown();
	TeardownTestSetup();

	return 0;
}
