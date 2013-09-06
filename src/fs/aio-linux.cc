#include <sys/syscall.h>
#include <fcntl.h>

#include "fs/aio-linux.h"
#include "schd/thread-pool.h"

using namespace dh_core;

//......................................................................... aio syscall wrapper ....

static long
io_setup(unsigned nr_reqs, aio_context_t *ctx)
{
	return syscall(__NR_io_setup, nr_reqs, ctx);
}

static long
io_destroy(aio_context_t ctx)
{
	return syscall(__NR_io_destroy, ctx);
}

static long
io_submit(aio_context_t ctx, long n, struct iocb **paiocb)
{
	return syscall(__NR_io_submit, ctx, n, paiocb);
}

static long
io_getevents(aio_context_t ctx, long min_nr, long nr, struct io_event *events,
	     struct timespec *tmo)
{
	return syscall(__NR_io_getevents, ctx, min_nr, nr, events, tmo);
}

//........................................................................... LinuxAioProcessor ....

LinuxAioProcessor::LinuxAioProcessor(const size_t nrthreads, const size_t nreqs)
	: log_("/linuxaioprocessor")
	, lock_("/linuxaioprocessor")
{
	Guard _(&lock_);

	/*
	 * Start polling threads
	 */
	for (size_t i = 0; i < nrthreads; ++i) {
		aio_context_t ctx;
		InitAioCtx(ctx, nreqs);
		ctxs_.push_back(ctx);
	}
}

void
LinuxAioProcessor::InitAioCtx(aio_context_t & ctx, const size_t nreqs)
{
	memset(&ctx, /*ch=*/ 0, sizeof(aio_context_t));
	long status = io_setup(nreqs, &ctx);
	INVARIANT(status != -1);

	INFO(log_) << "Starting aio thread ";

	PollThread * th = new PollThread(lock_, ctx, ops_);
	INVARIANT(th);

	aioths_.push_back(th);
	th->StartBlockingThread();
}

LinuxAioProcessor::~LinuxAioProcessor()
{
	Guard _(&lock_);

	/*
	 * stop and destroy threads
	 */
	for (auto it = aioths_.begin(); it != aioths_.end(); ++it) {
		PollThread * th = *it;
		th->Stop();
		delete th;

		INFO(log_) << "Destroyed aio thread.";
	}
	aioths_.clear();

	/* 
	 * close aio context
	 */
	for (auto it = ctxs_.begin(); it != ctxs_.end(); ++it) {
		aio_context_t & ctx = *it;
		long status = io_destroy(ctx);
		INVARIANT(status != -1);
	}
	ctxs_.clear();
}

void
LinuxAioProcessor::RegisterHandle(CHandle *)
{
	DEADEND
}

void
LinuxAioProcessor::UnregisterHandle(CHandle *, const AsyncProcessor::UnregisterDoneFn)
{
	DEADEND
}

int
LinuxAioProcessor::Write(Op * op)
{
	ASSERT(op->buf_.Ptr());
	ASSERT(op->size_);

	iocb & cb = op->iocb_;

	memset(&cb, 0, sizeof(iocb));
	cb.aio_fildes = op->fd_;
	cb.aio_lio_opcode = IOCB_CMD_PWRITE;
	cb.aio_reqprio = 0;
	cb.aio_buf = (u_int64_t) op->buf_.Ptr();
	cb.aio_nbytes = op->size_;
	cb.aio_offset = op->off_;
	cb.aio_data = (u_int64_t) op;

	op->piocb_[0] = &cb;

	{
		Guard _(&lock_);
		ops_.Push(op);
	}

	DEBUG(log_) << "io_submit: PWRITE op:" << (uint64_t) op
	            << " off: " << op->off_
		    << " size: " << op->size_;

	aio_context_t & ctx = ctxs_[rand() % ctxs_.size()];
	long status = io_submit(ctx, /*n=*/ 1, op->piocb_);

	if (status != 1) {
		ERROR(log_) << "Failed to submit io. PWRITE op: " << (uint64_t) op
			    << " strerror: " << strerror(errno);
	}

	return status;
}

int
LinuxAioProcessor::Read(Op * op)
{
	ASSERT(op->buf_.Ptr());
	ASSERT(op->size_);

	iocb & cb = op->iocb_;

	memset(&cb, 0, sizeof(iocb));
	cb.aio_fildes = op->fd_;
	cb.aio_lio_opcode = IOCB_CMD_PREAD;
	cb.aio_reqprio = 0;
	cb.aio_buf = (u_int64_t) op->buf_.Ptr();
	cb.aio_nbytes = op->size_;
	cb.aio_offset = op->off_;
	cb.aio_data = (u_int64_t) op;

	op->piocb_[0] = &cb;

	{
		Guard _(&lock_);
		ops_.Push(op);
	}

	DEBUG(log_) << "io_submit: PREAD op:" << (uint64_t) op
		    << " off: " << op->off_
		    << " size: " << op->size_;

	aio_context_t & ctx = ctxs_[rand() % ctxs_.size()];
	long status = io_submit(ctx, /*n=*/ 1, op->piocb_);

	if (status != 1) {
	    ERROR(log_) << "Failed to submit io. PREAD op: " << (uint64_t) op
		        << " strerror: " << strerror(errno);
	}

	return status;
}

// .............................................................. LinuxAioProcessor::PollThread ....

void *
LinuxAioProcessor::PollThread::ThreadMain()
{
	io_event events[LinuxAioProcessor::DEFAULT_MAX_EVENTS];

	while (true) {
		long status = io_getevents(ctx_, /*min_nr=*/ 1, sizeof(events), events,
					   /*timeout=*/ NULL);

		if (status == 0) {
			// no events returned
			continue;
		} else if (status < 0) {
			if (errno == EINTR) {
				// got interrupted by signal
				continue;
			}

			/*
			 * We don't handle other errors at this point
			 */
			ERROR(log_) << "Error reading events. err:" << strerror(errno);
			DEADEND
		}

		ASSERT(status > 0);

		DEBUG(log_) << "Got events. status:" << status;

		for (size_t i = 0; i < size_t(status); ++i) {
			const io_event & ev = events[i];

			DEBUG(log_) << "Event: data:" << ev.data
				    << " res:" << ev.res;

			Op * op = (Op *) ev.data;
			INVARIANT(&op->iocb_ == ((iocb *) ev.obj));

			{
				Guard _(&lock_);
				ops_.Unlink(op);
			}

			/*
			 * Callback interrupt
			 */
			op->ch_.Interrupt(int(ev.res), op);
		}
	}

	return NULL;
}

//.............................................................................. SpinningDevice ....

SpinningDevice::SpinningDevice(const std::string & devPath,
                               const disksize_t nsectors, AioProcessor * aio)
	: devPath_(devPath)
	, log_(STR("/spinningDevice") + devPath)
	, nsectors_(nsectors)
	, aio_(aio)
{
	ASSERT(aio);
	ASSERT(nsectors);

	INFO(log_)  << "SpinningDevice: path: " << devPath
		    << " nsectors: " << nsectors_;
}

SpinningDevice::~SpinningDevice()
{
	aio_ = NULL;
}

int
SpinningDevice::OpenDevice()
{
	fd_ = ::open(devPath_.c_str(), O_RDWR|O_CREAT|O_DIRECT, 0777);
	return fd_;
}

int
SpinningDevice::Write(const IOBuffer & buf, const diskoff_t off, const size_t nblks,
		      const Fn<int> & cb)
{
	INVARIANT((off + nblks) <= nsectors_);

	Op * op = new Op(fd_, buf, off * SECTOR_SIZE, nblks * SECTOR_SIZE,
			 intr_fn(this, &SpinningDevice::WriteDone), cb);

	int status = aio_->Write(op);
	return status;
}

int
SpinningDevice::Write(const IOBuffer & buf, const diskoff_t off, const size_t nblks)
{
    AsyncWait<int> waiter;

    Write(buf, off, nblks, intr_fn(&waiter, &AsyncWait<int>::Done));
    return waiter.Wait();
}

int
SpinningDevice::Read(IOBuffer & buf, const diskoff_t off, const size_t nblks,
		     const Fn<int> & cb)
{
	INVARIANT((off + nblks) <= nsectors_);

	Op * op = new Op(fd_, buf, off * SECTOR_SIZE, nblks * SECTOR_SIZE,
	                intr_fn(this, &SpinningDevice::WriteDone), cb);

	int status = aio_->Read(op);
	return status;
}

__interrupt__ void
SpinningDevice::WriteDone(int res, AioProcessor::Op * op)
{
	Op * wop = (Op *) op;

	wop->clientch_.Wakeup(res);

	/*
	 * we don't track the ops
	 */
	delete wop;
}
