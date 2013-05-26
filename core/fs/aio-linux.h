#ifndef _DH_CORE_FS_AIO_LINUX_H_
#define _DH_CORE_FS_AIO_LINUX_H_

#include <linux/aio_abi.h>

#include "core/async.h"
#include "core/logger.h"
#include "core/inlist.hpp"
#include "core/thread.h"

namespace dh_core {

//............................................................. BlockDevice ....

class BlockDevice
{
public:

    virtual ~BlockDevice() {}

    //.... pure virtual ....//

    virtual int Write(const IOBuffer & buf, const diskoff_t off,
                      const size_t size, const CompletionHandler<int> & h) = 0;

    virtual int Read(IOBuffer & buf, const diskoff_t off,
                     const size_t size, const CompletionHandler<int> & ch) = 0;
};

//............................................................ AioProcessor ....

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
           const size_t size, const CompletionHandler2<int, Op*> & ch)
            : fd_(fd), buf_(buf), off_(off), size_(size), ch_(ch)
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

//....................................................... LinuxAioProcessor ....

class LinuxAioProcessor : public AioProcessor, public Thread
{
public:

    static const size_t DEFAULT_MAX_EVENTS = 1024;

    LinuxAioProcessor(const size_t nreqs = DEFAULT_MAX_EVENTS);
    virtual ~LinuxAioProcessor();

    //.... static members ....//

    const uint32_t MAX_EVENTS = 1024;

    //.... AioProcessor override ....//

    virtual int Write(Op * op);
    virtual int Read(Op * op);

    //.... Thread override ....//

    virtual void * ThreadMain();

    //.... AsyncProcessor ....//

    void RegisterHandle(CHandle *);
    void UnregisterHandle(CHandle *, const AsyncProcessor::UnregisterDoneFn);

private:

    LogPath log_;
    SpinMutex lock_;
    aio_context_t ctx_;
    InList<Op> ops_;

};

//.......................................................... SpinningDevice ....

class SpinningDevice : public BlockDevice, public CHandle
{
public:

    //.... static members ....//

    static const uint64_t SECTOR_SIZE = 512; // 512 bytes

    //.... callback defs ....//

    typedef void (CHandle::*DoneFn)(int);

    //.... Create/destroy ....//

    SpinningDevice(const std::string & devPath, const disksize_t nsectors,
                   AioProcessor * aio);

    virtual ~SpinningDevice();

    //.... sync functions ....//

    int OpenDevice();

    //.... BlockDevice override ....//

    virtual int Write(const IOBuffer & buf, const diskoff_t off,
                      const size_t nblks, const CompletionHandler<int> & ch);
    virtual int Read(IOBuffer & buf, const diskoff_t off,
                     const size_t nblks, const CompletionHandler<int> & ch);

private:

    struct Op : AioProcessor::Op
    {
        Op(fd_t fd, const IOBuffer & buf, const diskoff_t off, const size_t size,
           const CompletionHandler2<int, AioProcessor::Op*> & opch,
           const CompletionHandler<int> & clientch)
            : AioProcessor::Op(fd, buf, off, size, opch), clientch_(clientch)
        {}

        CompletionHandler<int> clientch_;
    };

    //.... completion handlers ....//

    __interrupt__
    void WriteDone(int res, AioProcessor::Op * op);

    //.... private members ....//

    const std::string devPath_;
    LogPath log_;
    const uint64_t nsectors_;
    AioProcessor * aio_;
    fd_t fd_;
};


} // namespace dh_core {

#endif /*define _DH_CORE_FS_AIO_LINUX_H_ */
