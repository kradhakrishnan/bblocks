#ifndef _EXTENTFS_H_
#define _EXTENTFS_H_

namespace extentfs {

/*!
 *
 * /class   ExtentFs
 * /brief   Implementation of the extent file system
 *
 */
class ExtentFs
{
public:

    // .... create/destroy .... //

    explicit ExtentFs(BlockDevice * dev, const size_t pageSize);
    virtual ~ExtentFs();

    // .... async functions ....//

private:

    // .... Inaccessible .... //

    ExtentFs();
    ExtentFs(const ExtentFs &);
    operator=(const ExtentFs);

    // .... Member variables .... //

    BlockDevice dev_;
    const size_t pageSize_;
};

} // namespace extentfs

#endif /* _EXTENT_FS_H_ */
