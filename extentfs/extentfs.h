#ifndef _EXTENTFS_H_
#define _EXTENTFS_H_

#include "core/async.h"
#include "core/fs/aio-linux.h"
#include "extentfs/disklayout.h"
#include "extentfs/btree.h"

using namespace dh_core;

namespace extentfs {

class ExtentFs;
class FormatHelper;

/**
 * /class   ExtentFs
 * 
 * Implementation of the extent file system core.
 *
 */
class ExtentFs
{
public:

    friend class FormatHelper;
    friend class SuperblockHelper;

    // .... create/destroy .... //

    explicit ExtentFs(BlockDevice * dev, const size_t pageSize);
    virtual ~ExtentFs();

    // .... async functions ....//

    /**
     * Create a new extent file system instance and start serving.
     */
    void Create(const CHandler<int> & h /**< Callback */);

    /**
     * Open an exiting extent file system instance for serving.
     */
    void Open(const CHandler<int> & h /**< Callback */);

private:

    // .... Inaccessible .... //

    ExtentFs();
    ExtentFs(const ExtentFs &);
    void operator=(const ExtentFs &);

    // .... Internal functions .... //

    __completion_handler__
    void CreateDone(int status /**< Return status. -1 for error */);

    // .... Member variables .... //

    LogPath log_;
    BlockDevice * dev_;
    const size_t pageSize_;
    SuperBlock sb_;
};

/**
 * Help format the device for ExtentFs.
 *
 * Format the device with superblock and root node for the extent index.
 *
 */
class FormatHelper
{
public:

    FormatHelper(ExtentFs & fs /**< Extent file system reference */)
        : fs_(fs)
    {
    }

    /**
     * Format the specified device and create extent fs footprint.
     *
     */
    void Start(const bool isQuickFormat, CHandler<int> h);

private:

    ExtentFs & fs_;
};

/**
 * Help IO operations for superblock
 *
 * Encapsulates logical operations to superblock. Typically they include
 * reading, writing and manipulating content.
 */
class SuperblockHelper
{
public:

    SuperblockHelper(ExtentFs & fs)
        : fs_(fs)
    {
    }

private:

    ExtentFs & fs_;
};

} // namespace extentfs

#endif /* _EXTENT_FS_H_ */
