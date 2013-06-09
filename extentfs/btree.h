#ifndef _EXTENTFS_BPLUSTREE_H_
#define _EXTENTFS_BPLUSTREE_H_

#include "core/fs/aio-linux.h"
#include "extentfs/logoff.h"

using namespace dh_core;

namespace extentfs {

// ........................................................ BTreeIOProvider ....

/**
 * @class BTreeIOProvider
 *
 * IO Provider interface for BTree. We want to decouple memory management and IO
 * service from BTree. We want the BTree implementation focussed on the
 * processing logic and effeciency.
 *
 */
class BTreeIOProvider
{
public:

    /**
     * Allocate buffer for IO (usually from memory pool)
     */
    virtual void AllocBuffer(CHandler<IOBuffer> & cb /**< Callback */) = 0;

    virtual void Read(const LogOff & off, CHandler<IOBuffer> & cb) = 0;

    virtual void Write(const IOBuffer & buf, CHandler<LogOff> & cb) = 0;

    virtual void Update(const IOBuffer & buf, const LogOff & off,
                        CHandler<int> & cb) = 0;

};

// .................................................................. BTree ....

/**
 *  @class BTree
 *
 *  BTree implementation for LogFS
 *
 *  This class provides a B+Tree implementaiton designed for log structured file
 *  system. In a log structured file layout, records cannot be updated, hence
 *  when we write to the internal nodes, we need to write out all the nodes up
 *  to the root.
 *
 *  Theoretically, this changes the number of writes required to O(lg-t n)
 *  instead of O(1).
 *
 *  For our implementation, We will follow Introduction to Algorithms by Cormen.
 */
template<class _Key_, class _Val_>
class BTree
{
public:

    BTree(BTreeIOProvider * iomgr)
        : iomgr_(iomgr)
        , n_(0)
    {
        INVARIANT(iomgr_);
    }

    virtual ~BTree()
    {
        INVARIANT(iomgr_);
        iomgr_ = NULL;
    }

    /**
     * Create a new BTree on disk
     */
    void CreateTree(const LogOff & roff /**< Root node offset */);

    /**
     * Open an existing BTree from disk
     */
    void OpenTree(const LogOff & roff /**< Root node offset on disk*/);

private:

    /**
     * @struct Node
     *
     * BTree node representation
     *
     */
    struct Node
    {
        Node() : inMemory_(false) {}

        void Serialize(IOBuffer & buf);
        void Deserialize(const IOBuffer & buf);

        bool inMemory_;
        LogOff diskOff_;
        std::vector<_Key_> keys_;
        bool isLeaf_;

        union
        {
            std::vector<Node *> ptrs_;
            std::vector<_Val_> vals_;
        }
        data_;
    };

    BTreeIOProvider iomgr_;     ///< Disk IO provider for persistence
    Node root_;                 ///< Root of the tree
    uint64_t n_;                ///< Number of elements in the tree
    const size_t pageSize_;     ///< Page size on the block device
};


} // namespace dh_core


#endif /* _DHCORE_FS_LOGFS_BPLUSTREE_H_ */
