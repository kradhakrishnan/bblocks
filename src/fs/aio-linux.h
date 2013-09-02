#ifndef _FS_AIO_LINUX_H_
#define _FS_AIO_LINUX_H_

#include <linux/aio_abi.h>

#include "async.h"
#include "logger.h"
#include "inlist.hpp"
#include "thread.h"

namespace dh_core {

//................................................................................. BlockDevice ....

class BlockDevice
{
public:

	virtual ~BlockDevice() {}

	//.... pure virtual ....//

	virtual int Write(const IOBuffer & buf, const diskoff_t off, const size_t size,
			  const Fn<int> & h) = 0;

	virtual int Write(const IOBuffer & buf, const diskoff_t off, const size_t size) = 0;

	virtual int Read(IOBuffer & buf, const diskoff_t off, const size_t size,
			 const Fn<int> & ch) = 0;

	virtual disksize_t GetDeviceSize() = 0;
};

//................................................................................ AioProcessor ....

class AioProcessor : public AsyncProcessor
{
public:

	virtual ~AioProcessor() {}

	struct Op;

	//.... pure virtual ....//

	virtual int Write(Op * op) = 0;

	virtual int Read(Op * op) = 0;

	struct Op : public InListElement<Op>
	{
		Op(const fd_t fd, const IOBuffer & buf, const diskoff_t off,
		   const size_t size, const Fn2<int, Op*> & ch)
			: fd_(fd), buf_(buf), off_(off)
			, size_(size), ch_(ch)
		{}

		fd_t fd_;
		IOBuffer buf_;
		diskoff_t off_;
		size_t size_;
		CompletionHandler2<int, Op*> ch_;
		iocb iocb_;
		iocb * piocb_[1];
    };
};

//........................................................................... LinuxAioProcessor ....

class LinuxAioProcessor : public AioProcessor
{
public:

	static const size_t DEFAULT_NRTHREADS = 2; // ~1GBps
	static const size_t DEFAULT_MAX_EVENTS = 1024;

	//.... class PollThread ....//

	class PollThread : public Thread
	{
	public:

		PollThread(SpinMutex & lock, aio_context_t & ctx, InList<Op> & ops)
		    : Thread("/linuxaioprocessor/th/?")
		    , lock_(lock), ctx_(ctx), ops_(ops)
		{
			StartBlockingThread();
		}

		//.... Thread override ....//

		virtual void * ThreadMain();

	private:

		SpinMutex & lock_;
		aio_context_t ctx_;
		InList<Op> & ops_;
	};

	//.... create/destroy ....//

	LinuxAioProcessor(const size_t nrthreads = DEFAULT_NRTHREADS,
	                  const size_t nreqs = DEFAULT_MAX_EVENTS);
	virtual ~LinuxAioProcessor();

	//.... AioProcessor override ....//

	virtual int Write(Op * op);
	virtual int Read(Op * op);

	//.... AsyncProcessor ....//

	void RegisterHandle(CHandle *);
	void UnregisterHandle(CHandle *, const AsyncProcessor::UnregisterDoneFn);

private:

	void InitAioCtx(aio_context_t & ctx, const size_t nreqs);

	LogPath log_;
	SpinMutex lock_;
	std::vector<aio_context_t> ctxs_;
	std::vector<PollThread *> aioths_;
	InList<Op> ops_;
};

//.............................................................................. SpinningDevice ....

class SpinningDevice : public BlockDevice, public CHandle
{
public:

	//.... static members ....//

	static const uint64_t SECTOR_SIZE = 512; // 512 bytes

	//.... callback defs ....//

	typedef void (CHandle::*DoneFn)(int);

	//.... Create/destroy ....//

	SpinningDevice(const std::string & devPath, const disksize_t nsectors, AioProcessor * aio);

	virtual ~SpinningDevice();

	//.... sync functions ....//

	int OpenDevice();

	//.... BlockDevice override ....//

	virtual int Write(const IOBuffer & buf, const diskoff_t off, const size_t nblks,
		          const Fn<int> & ch);
	virtual int Write(const IOBuffer & buf, const diskoff_t off, const size_t nblks);
	virtual int Read(IOBuffer & buf, const diskoff_t off, const size_t nblks,
			 const Fn<int> & ch);

	virtual disksize_t GetDeviceSize()
	{
	    return nsectors_ * SECTOR_SIZE;
	}

private:

	struct Op : AioProcessor::Op
	{
		Op(fd_t fd, const IOBuffer & buf, const diskoff_t off, const size_t size,
		   const Fn2<int, AioProcessor::Op*> & opch, const Fn<int> & clientch)
			: AioProcessor::Op(fd, buf, off, size, opch)
			, clientch_(clientch)
		{}

		Fn<int> clientch_;
	};

	//.... completion handlers ....//

	__interrupt__ void WriteDone(int res, AioProcessor::Op * op);

	//.... private members ....//

	const std::string devPath_;
	LogPath log_;
	const uint64_t nsectors_;
	AioProcessor * aio_;
	fd_t fd_;
};


} // namespace dh_core {

#endif /*define _DH_CORE_FS_AIO_LINUX_H_ */
