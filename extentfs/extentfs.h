#ifndef _EXTENTFS_H_
#define _EXTENTFS_H_

#include "core/async.h"
#include "core/fs/aio-linux.h"
#include "extentfs/extentfs-disklayout.h"
#include "extentfs/btree.h"

using namespace dh_core;

namespace extentfs {

class ExtentFs;
class FormatHelper;
class SuperblockHelper;

// ........................................................... FormatHelper ....

/**
 * Help format the device for ExtentFs.
 *
 * Format the device with superblock and root node for the extent index.
 *
 */
class FormatHelper
{
public:

    FormatHelper(ExtentFs & fs) : log_("/efs/fmtHelper"), fs_(fs) {}

    /**
     * Format the specified device and create extent fs footprint.
     *
     */
    void Start(CHandler<int> chandler);

private:

    void LayoutSuperblock();

    LogPath log_;
    ExtentFs & fs_;
    CHandler<int> chandler_;
};

// ....................................................... SuperblockHelper ....

/**
 * Help IO operations for superblock
 *
 * Encapsulates logical operations to superblock. Typically they include
 * reading, writing and manipulating content.
 */
class SuperblockHelper
{
public:

    SuperblockHelper(ExtentFs & fs) : fs_(fs) {}

private:

    ExtentFs & fs_;
};

// ............................................................ ExtentIndex ....

class ExtentIndex
{
public:

    /* .... Create/destroy .... */

    explicit ExtentIndex(ExtentFs & fs);
    virtual ~ExtentIndex() {}

private:

    /* .... Member variables .... */

    ExtentFs & fs_;
};

// ............................................................... ExtentFs ....

/**
 * @class ExtentFs
 * 
 * Implementation of the extent file system core.
 *
 */
class ExtentFs
{
public:

    friend class FormatHelper;
    friend class SuperblockHelper;
    friend class ExtentIndex;

    // .... create/destroy .... //

    explicit ExtentFs(BlockDevice * dev, const size_t pageSize);
    virtual ~ExtentFs();

    // .... async functions ....//

    void Start(const bool newfs);

private:

    // .... Inaccessible .... //

    ExtentFs();
    ExtentFs(const ExtentFs &);
    void operator=(const ExtentFs &);

    // .... Internal functions .... //

    size_t nsuperblocks() const { return 2; }
    diskoff_t p2b(const diskoff_t pageoff)
    {
        return pageoff * (pageSize_ / 512);
    }

    void InitSuperblock(SuperBlock & sb, const bool cleanShutdown) const;
    void UpdateSuperblock(SuperBlock & sb, const bool cleanShutdown) const;

    void CreateStore();
    void OpenStore();

    __completion_handler__
    void CreateStoreDone(int status);

    // .... Member variables .... //

    SpinMutex lock_;
    LogPath log_;
    BlockDevice * dev_;
    const size_t pageSize_;
    const disksize_t npages_;
    LogAllocator logalloc_;
    SuperBlock sb_;

    LogOff lastWrite_;
    LogOff extentIndexOff_;
    LogOff extentMapOff_;

    // .... Helpers .... //

    FormatHelper fmtHelper_;
};


} // namespace extentfs

#endif /* _EXTENT_FS_H_ */
