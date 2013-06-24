#ifndef _EFS_TEST_BTREEHELPER_H_
#define _EFS_TEST_BTREEHELPER_H_

#include <map>

#include "extentfs/btree.h"

using std::map;

namespace extentfs {

class BTreeHelper : public BTreeIOProvider
{
public:

    /* .... BTreeIOProvider .... */

    virtual void AllocBuffer(const size_t size, CHandler<IOBuffer> & ch)
    {
        IOBuffer buf = IOBuffer::Alloc(size);
        ch.Wakeup(buf);
    }

    virtual void Read(const LogOff & off, CHandler<IOBuffer> & ch)
    {
        auto it = bufs_.find(off);
        INVARIANT(it != bufs_.end());
        ch.Wakeup(it->second);
    }

    virtual void Write(const IOBuffer & buf, CHandler<LogOff> & ch)
    {
        off_.off_ += 1;

        bool status = bufs_.insert(make_pair(off_, buf)).second;
        INVARIANT(status);

        ch.Wakeup(off_);
    }

private:

    LogOff off_;
    map<LogOff, IOBuffer> bufs_;
};

} // namespace extentfs

#endif /* #define _EFS_TEST_BTREEHELPER_H_ */
