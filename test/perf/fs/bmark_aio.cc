#include <boost/assert.hpp>
#include <cassert>
#include <boost/program_options.hpp>
#include <string>
#include <iostream>
#include <memory>

#include "fs/aio-linux.h"
#include "test/unit/unit-test.h"
#include "util.h"
#include "perf/perf-counter.h"

using namespace std;
using namespace bblocks;

namespace po = boost::program_options;

size_t _time_s = 10; // 10s

struct Stats
{
    Stats()
        : bytes_(0), time_usec_(0), count_(0)
    {}

    Timer ms_;
    atomic<uint64_t> bytes_;
    atomic<uint64_t> time_usec_;
    atomic<uint64_t> count_;
};

//................................................................................ AIOBenchamrk ....

/**
 * Benchmarking devices using LinuxAioProcessor implementation
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
		, devname_(devname)
		, devsize_(devsize)
		, iosize_(iosize)
		, iotype_(iotype)
		, iopattern_(iopattern)
		, qdepth_(qdepth)
		, aio_(new LinuxAioProcessor())
		, dev_(devname_, devsize_ / 512, aio_.Ptr())
		, buf_(IOBuffer::AllocMappedMem(iosize_))
		, nextOff_(UINT64_MAX)
		, pendingOps_(0)
		/* perf counters */
		, statLatency_("/aiobmark/latency", "microsec", PerfCounter::TIME) 
	{
		INVARIANT(!(iosize_ % 512));
		INVARIANT(!(devsize_ % 512));

		buf_.FillRandom();
	}

	virtual ~AIOBenchmark()
	{
		INFO(log_) << statLatency_;
	}

	void Start(int)
	{
		int status = dev_.OpenDevice();
		INVARIANT(status != -1);

		for (size_t i = 0; i < qdepth_; ++i) {
			IssueNextIO(new IOCtx);
		}
	}

private:

	struct IOCtx
	{
		IOCtx(const diskoff_t off = 0, const uint64_t start_usec = 0)
			: off_(off), start_usec_(start_usec)
		{}

		diskoff_t off_;
		uint64_t start_usec_;
	};

	void WriteDone(int status, IOCtx * ctx) /* inline callback */
	{
		ASSERT(ctx);

		DEBUG(log_) << "Write complete for " << ctx->off_;

		INVARIANT(status > 0 && size_t(status) == buf_.Size());

		UpdateStats(status, ctx);

		if (stats_.ms_.Elapsed() < (_time_s * 1000)) {
			IssueNextIO(ctx);
			pendingOps_--;
		} else {
			delete ctx;
			ctx = nullptr;

			pendingOps_--;
			if (!pendingOps_)
				Stop();
		}
	}

	void ReadDone(int status, IOCtx * ctx) /* inline callback */
	{
		ASSERT(ctx);

		DEBUG(log_) << "Read complete for " << ctx->off_;
 
		INVARIANT(status > 0 && size_t(status) == buf_.Size());

		UpdateStats(status, ctx);

		if (stats_.ms_.Elapsed() < (_time_s * 1000)) {
			IssueNextIO(ctx);
			pendingOps_--;
		} else {
			delete ctx;
			ctx = nullptr;

			pendingOps_--;
			if (!pendingOps_) Stop();
		}
	}

	void UpdateStats(int bytes, IOCtx * const ctx)
	{
		ASSERT(ctx);

		const uint64_t elapsed_usec = Time::ElapsedInMicroSec(ctx->start_usec_); 

		stats_.count_ += 1;
		stats_.bytes_ += bytes;
		stats_.time_usec_ += elapsed_usec;

		statLatency_.Update(elapsed_usec);
	}

	void IssueNextIO(IOCtx * const ctx)
	{
		pendingOps_++;

		diskoff_t off = NextOff();

		ctx->off_ = off;
		ctx->start_usec_ = Time::NowInMicroSec();

		if (iotype_ == READ) {
			auto fn = intr_fn(this, &AIOBenchmark::ReadDone, ctx);
			dev_.Read(buf_, off / 512, iosize_ / 512, fn);
		} else {
			auto fn = intr_fn(this, &AIOBenchmark::WriteDone, ctx);
			dev_.Write(buf_, off / 512, iosize_ / 512, fn);
		}
	}

	diskoff_t NextOff()
	{
		uint64_t expected = UINT64_MAX;
		if (nextOff_.compare_exchange_strong(expected, 0)) {
			return 0;
		}

		if (iopattern_ == SEQUENTIAL) {
			auto t = nextOff_.fetch_add(iosize_);
			// there can be a little glitch on wrap around.
			// todo: fix the glitch
			if ((t + iosize_) >= devsize_) nextOff_ = 0;
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

	template<class A, class B> 
	static double div(const A & a, const B & b)
	{
		return b ? (a / (double) b) : 0;
	}

	void PrintStats()
	{
		double MiB = stats_.bytes_ / (1024 * 1024);
		double s = stats_.ms_.Elapsed() / 1000;

		cout << "Result :" << endl
		     << "========" << endl
		     << " Total bytes written " << stats_.bytes_ << " B" << endl
		     << " Test time " << s << " s" << endl
		     << " Total ops " << stats_.count_ << endl
		     << " Op latency " << div(stats_.time_usec_, stats_.count_) << " usec" << endl
		     << " Ops/sec " << div(stats_.count_, s) << endl
		     << " MBps " << div(MiB, s) << endl;
	}

	string log_;
	const string devname_;
	const disksize_t devsize_;
	const size_t iosize_;
	const IOType iotype_;
	const IOPattern iopattern_;
	const size_t qdepth_;
	AutoPtr<AioProcessor> aio_;
	SpinningDevice dev_;
	IOBuffer buf_;
	atomic<diskoff_t> nextOff_;
	Stats stats_;
	atomic<size_t> pendingOps_;

	PerfCounter statLatency_;
};

//........................................................................................ Main ....

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

	try
	{
		po::store(po::parse_command_line(argc, argv, desc), parg);
		po::notify(parg);
	} catch (...) {
		cout << desc << endl;
		throw;
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
