#ifndef _EXTENTFS_DISKLAYOUT_H_
#define _EXTENTFS_DISKLAYOUT_H_

namespace extentfs {

#define SUPERBLOCK_MAGIC 0xdeeddeeddeeddeed
#define EXTENT_MAGIC 0xfeedfeedfeedfeed

#define RAID_STRIPE_SIZE (256 * 1024)
#define MAX_EXTENT_SIZE (256 * 1024)
#define MAX_BLKS_PER_EXTENT ((MAX_EXTENT_SIZE / 512) - 1)

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

    LogOff extentIndexRootOff_;
    LogOff extentMapOff_;
};


struct ExtentHeader
{
    // .... Identification .... //

    uint64_t magic_;
    uint64_t fsuuid_;
    uint32_t layoutVersion_;

    // .... Meta information .... //

    LogOff off_;
    LogOff prev_;
    uint64_t ctime_;
    uint64_t cksum_;

    // .... Block information .... //

    uint32_t nblks_;
    uint8_t blkbitmap_[MAX_BLKS_PER_EXTENT];
};


} // namespace extentfs

#endif /* _EXTENTFS_DISKLAYOUT_H_ */
