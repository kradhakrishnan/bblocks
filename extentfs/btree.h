#ifndef _EXTENTFS_BPLUSTREE_H_
#define _EXTENTFS_BPLUSTREE_H_

#include "core/fs/aio-linux.h"
#include "extentfs/logalloc.h"

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
    virtual void AllocBuffer(const size_t size, CHandler<IOBuffer> & cb) = 0;

    virtual void Read(const LogOff & off, CHandler<IOBuffer> & cb) = 0;

    virtual void Write(const IOBuffer & buf, CHandler<LogOff> & cb) = 0;
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
class BTree
{
public:

    typedef uint64_t key_t;

    /* .... Constants .... */

    static const size_t DEFAULT_NODE_FANOUT = 1024;
    static const size_t DEFAULT_HEADER_PADDING = 512;

    /* .... Create/destroy .... */

    explicit BTree(BTreeIOProvider & iomgr, const size_t pagesize)
        : log_("/btree")
        , t_(DEFAULT_NODE_FANOUT / 2)
        , pagesize_(CalcPageSize(pagesize))
        , iomgr_(iomgr)
        , root_(NULL)
    {
        INVARIANT(t_);
        INVARIANT(pagesize_);

        INFO(log_) << "BTree :"
                   << " t: " << t_
                   << " pagesize: " << pagesize_;
    }

    size_t CalcPageSize(const size_t pagesize)
    {
        // adjust the page size to hold t_ keys and values in it.
        size_t sz = ROUNDUP(sizeof(LeafNode), pagesize);
        // add an extra page to fit in the metadata
        sz += 1;

        return sz;
    }

    virtual ~BTree()
    {
    }

    /* .... Member functions .... */

    /**
     * Create a new BTree on disk
     */
    void Create(const LogOff & roff);

    /**
     * Open an existing BTree from disk
     */
    void OpenTree(const LogOff & roff);

private:

    struct Val
    {
        key_t key_;
        LogOff val_;
    };

    typedef Val val_t;

    /* .... Persistent data structure definition .... */

    /**
     * @struct PNode
     *
     * BTree node disk layout basics
     *
     */
    struct PNodeHeader
    {
        uint32_t magic_;
        uint32_t fsuuid_;
        uint32_t layoutVersion_;
        uint64_t mtime_;
        uint32_t n_;
        uint32_t pagesize_;
    };

    struct PNode : PNodeHeader
    {
        PNode(const bool isLeaf)
            : isLeaf_(isLeaf), n_(0)
        {}

        uint32_t isLeaf_;
        uint32_t n_;
    };

    /**
     * @struct InfoNode
     *
     * BTree internal node disk layout
     */
    struct PInfoNode : PNode
    {
        PInfoNode()
            : PNode(/*isLeaf=*/ false)
        {
            // memset(keys_, /*ch=*/ 0, sizeof(keys_) * sizeof(key_t));
            // memset(coffs_, /*ch=*/ 0, sizeof(coffs_) * sizeof(LogOff));
        }

        key_t keys_[DEFAULT_NODE_FANOUT];
        LogOff coffs_[DEFAULT_NODE_FANOUT];
    };

    /**
     * @struct PLeafNode
     *
     * BTree leaf node disk layout
     *
     */
    struct PLeafNode : PNode
    {
        PLeafNode()
            : PNode(/*isLeaf=*/ true)
        {
            // memset(vals_, /*ch=*/ 0, sizeof(vals_) * sizeof(val_t));
        }

        val_t vals_[DEFAULT_NODE_FANOUT];
    };

    /* .... In-memory data structure definition .... */

    struct Node
    {
        Node(bool isLeaf) : isLeaf_(isLeaf), n_(0), parent_(NULL) {}

        bool isLeaf_;
        uint32_t n_;
        Node * parent_;
    };

    struct InfoNode : Node
    {
        InfoNode()
            : Node(/*isLeaf=*/ false)
            , pnode_(new PInfoNode())
        {
            // memset(cnodes_, /*ch=*/ 0, sizeof(cnodes_) * sizeof(Node *));
        }

        PInfoNode * pnode_;
        Node * cnodes_[DEFAULT_NODE_FANOUT];
    };

    struct LeafNode : Node
    {
        LeafNode()
            : Node(/*isLeaf=*/ true)
            , pnode_(new PLeafNode())
        {}

        PLeafNode * pnode_;
    };

    /* .... BTree in-memory operations .... */

    void CreateTree();
    void Insert(const key_t & key, const val_t & val);
    void SplitNode(InfoNode * r, const size_t idx, Node * c);
    void InsertNonNull(Node * n, const key_t & key, const val_t & val);

    Node * Split(Node * n);
    InfoNode * SplitInfoNode(InfoNode * n);
    LeafNode * SplitLeafNode(LeafNode * n);

    /* .... Member variables .... */

    LogPath log_;
    const size_t t_;
    const size_t pagesize_;
    BTreeIOProvider & iomgr_;
    Node * root_;
};

} // namespace dh_core


#endif /* _DHCORE_FS_LOGFS_BPLUSTREE_H_ */
