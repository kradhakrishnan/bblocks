#include <iostream>
#include <boost/program_options.hpp>
#include <string>

#include "core/defs.h"
#include "extentfs/extentfs.h"
#include "extentfs/disklayout.h"
#include "extentfs/btree.h"

using namespace std;
using namespace extentfs;
using namespace dh_core;

namespace po = boost::program_options;

static const uint32_t DEFAULT_LAYOUT_VERSION = 0;

// ............................................................... ExtentFs ....

ExtentFs::ExtentFs(BlockDevice * dev, const size_t pageSize)
    :
    log_("/extentFs")
    , dev_(dev)
    , pageSize_(pageSize)
    , npages_(dev_->GetDeviceSize() / pageSize_)
    , logalloc_(pageSize_, npages_, nsuperblocks() / 2,
                npages_ - (nsuperblocks() / 2))
    /* Helpers */
    , fmtHelper_(*this)
{
    INFO(log_) << "ExtentFs :"
               << " dev: " << dev_
               << " pageSize: " << pageSize_
               << " npages: " << npages_;


    // Verify alignment
    INVARIANT(npages_ * pageSize_ == dev->GetDeviceSize());
    INVARIANT(!(nsuperblocks() % 2));
}

ExtentFs::~ExtentFs()
{
    dev_ = NULL;
}

void
ExtentFs::CreateStore()
{
    ThreadPool::Schedule(&fmtHelper_, &FormatHelper::Start,
                         async_fn(this, &ExtentFs::CreateStoreDone));
}

void
ExtentFs::CreateStoreDone(int status)
{
    DEADEND
}

void
ExtentFs::Start(const bool newfs)
{
    if (newfs) {
        // this is a new file system, we need to format the disk first
        CreateStore();
        return;
    }

    OpenStore();
}

void
ExtentFs::OpenStore()
{
}

void
ExtentFs::InitSuperblock(SuperBlock & sb, const bool cleanShutdown) const
{
    sb.magic_ = SUPERBLOCK_MAGIC;
    sb.fsuuid_ = rand();
    sb.layoutVersion_ = DEFAULT_LAYOUT_VERSION;
    sb.sbVersion_ = 0;
    sb.ctime_ = Time::NowInMilliSec();
    sb.mtime_ = Time::NowInMilliSec();
    sb.accessFlag_ = 0;
    sb.cleanShutdown_ = cleanShutdown;
    sb.devSize_ = dev_->GetDeviceSize();
    sb.pageSize_ = pageSize_;
    sb.npages_ = npages_;
    sb.lastWrite_ = LogOff();
    sb.extentIndexOff_ = LogOff(/*wrap=*/ 0, /*off=*/ 1);
    sb.extentMapOff_ = LogOff();

    memset(sb.pad_, /*ch=*/ 0, sizeof(sb.pad_));

    sb.cksum_ = 0;
    sb.cksum_ = Adler32::Calc((uint8_t *) &sb, sizeof(sb));
}

void
ExtentFs::UpdateSuperblock(SuperBlock & sb, const bool cleanShutdown) const
{
    INVARIANT(sb.magic_ == SUPERBLOCK_MAGIC);
    INVARIANT(sb.layoutVersion_ == DEFAULT_LAYOUT_VERSION);
    INVARIANT(sb.accessFlag_ == 0);
    INVARIANT(sb.devSize_ == dev_->GetDeviceSize());
    INVARIANT(sb.pageSize_ == pageSize_);
    INVARIANT(sb.npages_ == npages_);

    sb.sbVersion_ += sb.sbVersion_;
    sb.ctime_ = Time::NowInMilliSec();
    sb.mtime_ = Time::NowInMilliSec();
    sb.cleanShutdown_ = cleanShutdown;

    memset(sb.pad_, /*ch=*/ 0, sizeof(sb.pad_));

    sb.cksum_ = 0;
    sb.cksum_ = Adler32::Calc((uint8_t *) &sb, sizeof(sb));
}


// ........................................................... FormatHelper ....

void
FormatHelper::Start(CHandler<int> chandler)
{
    Guard _(&fs_.lock_);

    chandler_ = chandler;

    LayoutSuperblock();
}

void
FormatHelper::LayoutSuperblock()
{
    ASSERT(fs_.lock_.IsOwner());

    fs_.InitSuperblock(fs_.sb_);

    //
    // write superblock 0
    //
    IOBuffer buf = IOBuffer::Alloc(fs_.pageSize_);
    buf.Fill(/*ch=*/ 0);
    buf.Copy(fs_.sb_);

    int status = fs_.dev_->Write(buf, /*off=*/ 0, buf.Size() / 512);

    if ((size_t) status != fs_.pageSize_) {
        // error writing superblock to disk
        ERROR(log_) << "Error writing superblock 0. errno: " << -status;
        chandler_.Wakeup(-1);
        return;
    }

    DEFENSIVE_CHECK((size_t) status == fs_.pageSize_);

    //
    // Write superblock n
    //
    fs_.UpdateSuperblock(fs_.sb_);

    buf.Fill(/*ch=*/ 0);
    buf.Copy(fs_.sb_);

    status = fs_.dev_->Write(buf, /*off=*/ fs_.p2b(fs_.npages_ - 1),
                             buf.Size() / 512);

    if ((size_t) status != fs_.pageSize_) {
        // error writing superblock to disk
        ERROR(log_) << "Error writing superblock at "
                    << fs_.npages_ - 1 << ". errno: " << -status;
        chandler_.Wakeup(-1);
        return;
    }

    DEFENSIVE_CHECK((size_t) status == fs_.pageSize_);

    chandler_.Wakeup(1);
}
