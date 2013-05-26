#include <sys/syscall.h>
#include <fcntl.h>

#include "core/fs/aio-linux.h"
#include "core/thread-pool.h"

using namespace dh_core;

//..................................................... aio syscall wrapper ....

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
io_getevents(aio_context_t ctx, long min_nr, long nr,
             struct io_event *events, struct timespec *tmo)
{
    return syscall(__NR_io_getevents, ctx, min_nr, nr, events, tmo);
}

//....................................................... LinuxAioProcessor ....

LinuxAioProcessor::LinuxAioProcessor(const size_t nreqs)
    : Thread("/linuxaioprocessor/th")
    , log_("/linuxaioprocessor")
{
    AutoLock _(&lock_);

    memset(&ctx_, /*ch=*/ 0, sizeof(ctx_));
    long status = io_setup(nreqs, &ctx_);
    INVARIANT(status != -1);

    StartBlockingThread();
}

LinuxAioProcessor::~LinuxAioProcessor()
{
    AutoLock _(&lock_);

    long status = io_destroy(ctx_);
    INVARIANT(status != -1);
}

void
LinuxAioProcessor::RegisterHandle(CHandle *)
{
    DEADEND
}

void
LinuxAioProcessor::UnregisterHandle(CHandle *,
                                const AsyncProcessor::UnregisterDoneFn)
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
        AutoLock _(&lock_);
        ops_.Push(op);
    }

    DEBUG(log_) << "io_submit: PWRITE op:" << (uint64_t) op
                << " off: " << op->off_
                << " size: " << op->size_;

    long status = io_submit(ctx_, /*n=*/ 1, op->piocb_);

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
        AutoLock _(&lock_);
        ops_.Push(op);
    }

    DEBUG(log_) << "io_submit: PREAD op:" << (uint64_t) op
                << " off: " << op->off_
                << " size: " << op->size_;

    long status = io_submit(ctx_, /*n=*/ 1, op->piocb_);

    if (status != 1) {
        ERROR(log_) << "Failed to submit io. PREAD op: " << (uint64_t) op
                    << " strerror: " << strerror(errno);
    }

    return status;
}

void *
LinuxAioProcessor::ThreadMain()
{
    io_event events[MAX_EVENTS];

    while (true) {
        long status = io_getevents(ctx_, /*min_nr=*/ 1, MAX_EVENTS, events,
                                   /*timeout=*/ NULL);

        if (status == 0) {
            // no events returned
            DEBUG(log_) << "No events returned";
            continue;
        } else if (status < 0) {
            if (errno == EINTR) {
                // got interrupted by signal
                continue;
            }

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
                AutoLock _(&lock_);
                ops_.Unlink(op);
            }

            // callback as interrupt for perf reasons
            op->ch_.Interrupt(int(ev.res), op);
       }
    }

    return NULL;
}

//.......................................................... SpinningDevice ....

SpinningDevice::SpinningDevice(const std::string & devPath,
                               const disksize_t nsectors, AioProcessor * aio)
    : devPath_(devPath)
    , log_(STR("/spinningDevice") + devPath)
    , nsectors_(nsectors)
    , aio_(aio)
{
    ASSERT(aio);
    ASSERT(nsectors);
}

SpinningDevice::~SpinningDevice()
{
    aio_ = NULL;
}

int
SpinningDevice::OpenDevice()
{
    fd_ = ::open(devPath_.c_str(), O_RDWR|O_CREAT, 0777);
    return fd_;
}

int
SpinningDevice::Write(const IOBuffer & buf, const diskoff_t off,
                      const size_t nblks, const CompletionHandler<int> & cb)
{
    ASSERT((off + nblks) * SECTOR_SIZE < nsectors_);

    Op * op = new Op(fd_, buf, off * SECTOR_SIZE, nblks * SECTOR_SIZE,
                     intr_fn(this, &SpinningDevice::WriteDone), cb);

    int status = aio_->Write(op);

    return status;
}

int
SpinningDevice::Read(IOBuffer & buf, const diskoff_t off,
                     const size_t nblks, const CompletionHandler<int> & cb)
{
    ASSERT((off + nblks) * SECTOR_SIZE < nsectors_);

    Op * op = new Op(fd_, buf, off * SECTOR_SIZE, nblks * SECTOR_SIZE,
                     intr_fn(this, &SpinningDevice::WriteDone), cb);

    int status = aio_->Read(op);

    return status;
}

__interrupt__
void
SpinningDevice::WriteDone(int res, AioProcessor::Op * op)
{
    Op * wop = (Op *) op;

    wop->clientch_.Wakeup(res);

    // we don't track the ops, so delete
    delete wop;
}

