#include "extentfs/logalloc.h"

using namespace extentfs;

std::ostream &
operator<<(std::ostream & os, const LogOff & off)
{
    os << "LogOff: wrap=" << wrap_ << " off=" << off_;
    return os;
}

// ................................................................. LogOff ....

LogOff
LogAllocator::Next(const size_t size, LogOff & prev)
{
    prev = off_;

    if (off_.wrap_ == 0 && off_.off_ + size < endoff_) {
        // first wrap, linear allocation
        Inc(off_, size);
        INVARIANT(off_.wrap_ == 0);
        return;
    }

    // not the first wrap need, to allocate based on extent availability
    DEADEND
}
