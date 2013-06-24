#include "extentfs/btree.h"

using namespace extentfs;

void
BTree::Create(const LogOff & off)
{
    // init the in-memory data structure
    CreateTree();

    // write it down to disk

    // sync the superblock
}

void
BTree::CreateTree()
{
    INVARIANT(!root_);
    root_ = new LeafNode();
}

void
BTree::Insert(const key_t & key, const val_t & val)
{
    INVARIANT(root_);
    Node * r = root_;

    if (root_->n_ == (2 * t_ - 1)) {
        // root node is full
        InfoNode * s = new InfoNode();
        root_ = r;
        s->cnodes_[0] = r;
        r->parent_ = s;
        SplitNode(s, /*off=*/ 0, r);
    }

    InsertNonNull(root_, key, val);
}

void
BTree::SplitNode(InfoNode * r, const size_t idx, Node * c)
{
    INVARIANT(r);
    INVARIANT(c);
    INVARIANT(idx < (2 * t_ - 1));

    Node * cright = Split(c);
    r->cnodes_[idx + 1] = cright;
}

BTree::Node *
BTree::Split(Node * n)
{
    INVARIANT(n->n_ == (2 * t_) - 1);

    if (n->isLeaf_) {
        return SplitLeafNode(static_cast<LeafNode *>(n));
    }

    return SplitInfoNode(static_cast<InfoNode *>(n));
}

BTree::InfoNode *
BTree::SplitInfoNode(InfoNode * n)
{
    INVARIANT(n->n_ == (2 * t_) - 1);

    InfoNode * s = new InfoNode();

    // copy over pnode and cnode data
    size_t i, j;
    for (i = n->n_ - 1, j = 0; i > t_; --i, j++) {
        s->pnode_->keys_[j] = n->pnode_->keys_[i];
        s->cnodes_[j] = n->cnodes_[i];

        n->cnodes_[i] = NULL;
        n->n_--;
        s->n_++;
    }

    INVARIANT(n->n_ == t_);
    INVARIANT(s->n_ == t_ - 1);

    return s;
}

BTree::LeafNode *
BTree::SplitLeafNode(LeafNode * n)
{
    INVARIANT(n->n_ == (2 * t_) - 1);

    LeafNode * s = new LeafNode();

    // copy over half the data
    size_t i, j;
    for (i = n->n_ - 1, j = 0; i > t_; i--, j++) {
        s->pnode_->vals_[j] = n->pnode_->vals_[i];

        n->n_--;
        s->n_++;
    }

    INVARIANT(n->n_ == t_);
    INVARIANT(s->n_ == t_ - 1);

    return s;
}

void
BTree::InsertNonNull(Node * n, const key_t & key, const val_t & val)
{
/*
    if (n->isLeaf_) {
        InsertToLeaf(static_cast<LeafNode *>(n), key, val);
        return;
    }

    DEFENSIVE_CHECK(!n->isLeaf_);

    InfoNode * inode = static_cast<InfoNode *>(n);

    size_t idx = FindKey(inode, key);

    INVARIANT(idx < (2 * t_) - 1);

    if (inode->cnodes_[idx]) {
        // the node is in memory
    } else {
        // need to read this node from disk
        DEADEND
    }
*/
}
