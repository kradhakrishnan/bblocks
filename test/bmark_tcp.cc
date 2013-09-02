#include <boost/program_options.hpp>
#include <string>
#include <iostream>

#include "net/tcp-linux.h"
#include "test/unit-test.h"

using namespace std;
using namespace dh_core;

namespace po = boost::program_options;

// log path
static LogPath _log("/bmark_tcp");

//
// Channel stat abstraction
//
struct ChStats
{
	ChStats()
		: start_ms_(NowInMilliSec()), bytes_read_(0), bytes_written_(0)
	{}

	uint64_t start_ms_;
	uint64_t bytes_read_;
	uint64_t bytes_written_;
};

//...................................................... TCPServerBenchmark ....

///
/// @class TCPServerBenchmark
///
/// Server implementation of TCP benchmark
///
class TCPServerBenchmark : public CompletionHandle
{
public:

	typedef TCPServerBenchmark This;

	/*
	 * create/destroy
	 */
	TCPServerBenchmark(const size_t iosize)
		: lock_("/server")
		, epoll_("/server")
		, server_(epoll_)
		, buf_(IOBuffer::Alloc(iosize))
		, rcqueue_(this, &This::ReadDone)
	{}

	virtual ~TCPServerBenchmark() {}

	//
	// Start listening
	//
	void Start(sockaddr_in addr)
	{
		INFO(_log) << "Starting to listen";

		auto fn = async_fn(this, &This::HandleServerConn);
		const bool ok = server_.Listen(addr, fn);
		INVARIANT(ok);
	}

	/*
	 * Completion handlers
	 */

	//
	// Connection established callback
	//
	__completion_handler__
	virtual void HandleServerConn(TCPServer *, int status, TCPChannel * ch)
	{
		INFO(_log) << "Got ch " << ch;

		/*
		 * Create channel stat node
		 */
		{
		    Guard _(&lock_);
		    chstats_.insert(make_pair(ch, ChStats()));
		}

		ch->RegisterHandle(this);
		ThreadPool::Schedule(this, &This::ReadUntilBlocked, ch);
	}

	//
	// Read data until blocked
	//
	void ReadUntilBlocked(TCPChannel * ch)
	{
		while (ch->Read(buf_, cqueue_fn(&rcqueue_))) {
			UpdateStats(ch, buf_.Size());

			if (ThreadPool::ShouldYield()) {
				ThreadPool::Schedule(this,
					    &This::ReadUntilBlocked, ch);
				return;
			}
		}
	}

	//
	// Read completion callback
	//
	__completion_handler__
	virtual void ReadDone(TCPChannel * ch, int status, IOBuffer)
	{
		/*
		 * Update stats
		 */
		ASSERT((size_t) status == buf_.Size());
		UpdateStats(ch, buf_.Size());

		ReadUntilBlocked(ch);
	}

private:

	void UpdateStats(TCPChannel * ch, const uint32_t size)
	{
		Guard _(&lock_);

		auto it = chstats_.find(ch);
		ASSERT(it != chstats_.end());

		it->second.bytes_read_ += size;

		uint64_t elapsed_s = MS2SEC(Timer::Elapsed(it->second.start_ms_));
		if (elapsed_s >= 5) {
			/* Print results on screen */
			double MB = it->second.bytes_read_ / double(1024 * 1024);
			double MBps = MB / (double) elapsed_s;
				INFO(_log) << "ch " << (uint64_t) it->first
					   << " bytes " << it->second.bytes_read_
					   << " MBps " << MBps;

			/* Reset print timer */
			it->second.bytes_read_ = 0;
			it->second.start_ms_ = NowInMilliSec();
		}
	}

	typedef map<TCPChannel *, ChStats> chstats_map_t;

	SpinMutex lock_;
	Epoll epoll_;
	TCPServer server_;
	chstats_map_t chstats_;
	IOBuffer buf_;
	CQueue3<TCPChannel *, int, IOBuffer> rcqueue_;
};

//...................................................... TCPClientBenchmark ....

///
/// @class TCPClientBenchmark
///
/// Client implementation of TCP benchmark
///
class TCPClientBenchmark : public CompletionHandle
{
public:

    typedef TCPClientBenchmark This;

    /*.... create/destroy ....*/

    TCPClientBenchmark(const SocketAddress & addr, const size_t iosize,
                       const size_t nconn, const size_t nsec)
        : lock_("/client")
        , epoll_("/client")
        , connector_(epoll_)
        , addr_(addr)
        , iosize_(iosize)
        , nconn_(nconn)
        , nsec_(nsec)
        , buf_(IOBuffer::Alloc(iosize))
        , wcqueue_(this, &This::WriteDone)
    {}

    virtual ~TCPClientBenchmark()
    {
        Guard _(&lock_);

        buf_.Trash();

        for (auto chstat : chstats_) {
            TCPChannel * ch = chstat.first;
            ch->Close();
            delete ch;
        }

        chstats_.clear();
   }

    void Start(int)
    {
        Guard _(&lock_);

        for (size_t i = 0; i < nconn_; ++i) {
            pendingConns_.Add(/*count=*/ 1);
            connector_.Connect(addr_, async_fn(this, &This::Connected));
        }
    }

    /* .... handler functions .... */

    __completion_handler__
    virtual void WriteDone(TCPChannel * ch, int status)
    {
        ASSERT(status);
        UpdateStats(ch, status);

        pendingios_.Add(/*val=*/ -1);

        SendData(ch);
    }

    /* .... callbacks .... */

    __completion_handler__
    void Connected(TCPConnector *, int status, TCPChannel * ch)
    {
        ASSERT(status == OK);
        ASSERT(buf_.Size() == iosize_);

        {
            Guard _(&lock_);
            chstats_.insert(make_pair(ch, ChStats()));
        }

        INVARIANT(pendingConns_.Count());

        ch->RegisterHandle(this);
        ch->SetWriteDoneFn(cqueue_fn(&wcqueue_));

        pendingConns_.Add(/*val=*/ -1);
        nactiveconn_.Add(/*val=*/ 1);

        SendData(ch);
    }

    __interrupt__
    void Unregistered(int status)
    {
        INFO(_log) << "Unregistered.";

        if (nactiveconn_.Add(/*val=*/ -1) == 1) {
            // all clients disconnected
            ThreadPool::Schedule(this, &This::Halt, /*val=*/ 0);
        }
    }

private:

    void Stop()
    {
        ASSERT(timer_.Elapsed() > SEC2MS(nsec_));

        if (!pendingios_.Count() && !pendingConns_.Count()) {
            INFO(_log) << "Stopping channels.";

            Guard _(&lock_);
            for (auto chstat : chstats_) {
                TCPChannel * ch = chstat.first;
                ch->UnregisterHandle(static_cast<CHandle *>(this),
                                     async_fn(&This::Unregistered));
            }
        }
    }

    void Halt(int)
    {
        PrintStats();
        ThreadPool::Shutdown();
    }

    void UpdateStats(TCPChannel * ch, const int bytes_written)
    {
        Guard _(&lock_);

        auto it = chstats_.find(ch);
        ASSERT(it != chstats_.end());
        it->second.bytes_written_ += bytes_written;
    }

    void SendData(TCPChannel * ch)
    {
        int status;
        while (true) {
            if (timer_.Elapsed() > SEC2MS(nsec_)) {
                // time elapsed, stop sending data
                Stop();
                break;
            }

            pendingios_.Add(/*val=*/ 1);

            status = ch->EnqueueWrite(buf_);
            INVARIANT(status == -EBUSY || size_t(status) <= buf_.Size());

            if (status == -EBUSY) {
                pendingios_.Add(/*val=*/ -1); 
                break;
            } else if (size_t(status) == buf_.Size()) {
                UpdateStats(ch, status);
                pendingios_.Add(/*val=*/ -1);
            } else {
                // operate in serial mode, ideally suited for most applications
                // unless the IO is too small
                // TODO: Provide a config param
                break;
            }

            if (ThreadPool::ShouldYield()) {
                ThreadPool::Schedule(this, &This::SendData, ch);
                break;
            }
        }
    }

    void PrintStats()
    {
        Guard _(&lock_);

        for (auto stat : chstats_) {
            auto MBps = B2MB(stat.second.bytes_written_) /
                                                MS2SEC(timer_.Elapsed());
            INFO(_log) << "Channel " << stat.first << " :"
                       << " w-bytes " << stat.second.bytes_written_ << " bytes"
                       << " time : " << MS2SEC(timer_.Elapsed()) << " s"
                       << " write throughput : " << MBps << " MBps";
        }
    }

    typedef map<TCPChannel *, ChStats> chstats_map_t;

    SpinMutex lock_;
    Epoll epoll_;
    TCPConnector connector_;
    const SocketAddress addr_;
    const size_t iosize_;
    const size_t nconn_;
    const size_t nsec_;
    chstats_map_t chstats_;     //< Stats holder
    IOBuffer buf_;
    Timer timer_;
    AtomicCounter nactiveconn_;
    AtomicCounter pendingios_;
    AtomicCounter pendingConns_;
    CQueue2<TCPChannel *, int> wcqueue_;
};


//.................................................................... Main ....

int
main(int argc, char ** argv)
{
	string laddr = "0.0.0.0:0";
	string raddr;
	int iosize = 4 * 1024;
	int nconn = 1;
	int seconds = 60;
	int ncpu = 8;

	po::options_description desc("Options:");
	desc.add_options()
		("help",    "Print usage")
		("server",  "Start server component")
		("client",  "Start client component")
		("laddr",   po::value<string>(&laddr),
			    "Local address (Default INADDR_ANY:0)")
		("raddr",   po::value<string>(&raddr),
			    "Remote address")
		("iosize",  po::value<int>(&iosize),
			    "IO size in bytes")
		("conn",    po::value<int>(&nconn),
			    "Client connections (Default 1)")
		("s",	    po::value(&seconds),
			    "Time in sec (only with -c)")
		("ncpu",    po::value<int>(&ncpu),
			    "CPUs to use");

	po::variables_map parg;

	try {
		po::store(po::parse_command_line(argc, argv, desc), parg);
		po::notify(parg);
	} catch (...) {
		cerr << "Error parsing command arguments." << endl;
		cout << desc << endl;
		return -1;
	}

	const bool showusage = parg.count("help")
			    || (parg.count("server") && parg.count("client"))
			    || (!parg.count("server") && !parg.count("client"));

	if (showusage) {
		cout << desc << endl;
		return -1;
	}

	const bool isClientBenchmark = parg.count("client");

	InitTestSetup();
	ThreadPool::Start(ncpu);

	if (isClientBenchmark) {
		INFO(_log) << "Running benchmark for"
			   << " address " << laddr << "->" << raddr
			   << " iosize " << iosize << " bytes"
			   << " nconn " << nconn
			   << " ncpu " << ncpu
			   << " seconds " << seconds << " s";

		ASSERT(!raddr.empty());
		SocketAddress addr = SocketAddress::GetAddr(laddr, raddr);
		TCPClientBenchmark c(addr, iosize, nconn, seconds);
		ThreadPool::Schedule(&c, &TCPClientBenchmark::Start,
				    /*status=*/ 0);
		ThreadPool::Wait();
	} else {
		INFO(_log) << "Running server at " << laddr
			   << " ncpu " << ncpu;

		ASSERT(!laddr.empty());
		TCPServerBenchmark s(iosize);
		ThreadPool::Schedule(&s, &TCPServerBenchmark::Start,
			             SocketAddress::GetAddr(laddr));
		ThreadPool::Wait();
	}

	TeardownTestSetup();
	return 0;
}
