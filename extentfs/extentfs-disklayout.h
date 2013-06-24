#ifndef _EXTENTFS_DISKLAYOUT_H_
#define _EXTENTFS_DISKLAYOUT_H_

#include "core/defs.h"
#include "extentfs/logalloc.h"

using namespace dh_core;

namespace extentfs {

#define SUPERBLOCK_MAGIC 0xdeeddeeddeeddeed
#define EXTENT_MAGIC 0xfeedfeedfeedfeed

#define RAID_STRIPE_SIZE KiB(256)
#define MAX_EXTENT_SIZE KiB(256)
#define MAX_BLKS_PER_EXTENT ((MAX_EXTENT_SIZE / 512) - 1)
#define PAGE_SIZE_FACTOR KiB(4)

struct SuperBlock
{
    // .... Identification .... //

    uint64_t magic_;
    uint64_t fsuuid_;
    uint32_t layoutVersion_;
    uint64_t sbVersion_;

    // .... Meta information .... //

    uint64_t ctime_;
    uint64_t mtime_;
    uint64_t cksum_;
    uint64_t accessFlag_;
    uint64_t cleanShutdown_;

    // .... Geometry .... //

    uint64_t devSize_;
    uint32_t pageSize_;
    uint64_t npages_;

    // .... Disk Ptrs ... //

    LogOff lastWrite_;

    // .... Special Block Index .... //

    LogOff extentIndexOff_;
    LogOff extentMapOff_;

    // .... Padding .... //

    uint8_t pad_[368];
};

static_assert(sizeof(SuperBlock) == 512, "Superblock not aligned");

struct ExtentHeader
{
    // .... Identification .... //

    uint64_t magic_;
    uint64_t fsuuid_;
    uint32_t layoutVersion_;

    // .... Meta information .... //

    LogOff off_;
    LogOff prevOff_;
    uint64_t ctime_;
    uint64_t cksum_;

    // .... Block information .... //

    uint32_t nbytes_;

    // .... Padding .... //

    uint8_t pad_[432];
};

static_assert(sizeof(ExtentHeader) == 512, "ExtentHeader not aligned");

} // namespace extentfs

#endif /* _EXTENTFS_DISKLAYOUT_H_ */
