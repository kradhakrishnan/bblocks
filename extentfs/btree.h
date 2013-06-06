#ifndef _DHCORE_FS_LOGFS_BPLUSTREE_H_
#define _DHCORE_FS_LOGFS_BPLUSTREE_H_

#include "core/fs/aio-linux.h"
#include "extentfs/logoff.h"

namespace dh_core {

// ............................................................. IOProvider ....

/*!
 *  \class BTree
 *  \brief BTree implementation for LogFS
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

    /*!
     * \brief Create a new BTree on disk
     */
    void CreateTree();

    /*!
     * \brief Open an existing BTree from disk
     *
     * \param   off     Location of root node
     */
    void OpenTree(const LogOff & off);

private:

    /*!
     * \class Node
     * \brief BTree node
     */
    struct Node
    {
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

    Node root_;                 // Root of the tree
    uint64_t n_;                // Number of elements in the tree
    BlockDevice * dev_;         // Blockdevice to write to
    const size_t pageSize_;     // Page size on the block device
};


} // namespace dh_core


#endif /* _DHCORE_FS_LOGFS_BPLUSTREE_H_ */
