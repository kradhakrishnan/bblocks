#ifndef _EXTENTFS_LOFF_ALLOCATOR_H_
#define _EXTENTFS_LOFF_ALLOCATOR_H_

#include "core/logger.h"
#include "core/defs.h"

using namespace dh_core;

namespace extentfs {

class LogAllocator;

// ................................................................. LogOff ....

/**
 * @class   LogOff
 *
 * Offset representation for log structure layout
 */
struct LogOff
{
    LogOff() : wrap_(UINT32_MAX), off_(UINT64_MAX) {}

    LogOff(const uint32_t wrap, const uint64_t off)
        : wrap_(wrap), off_(off)
    {}

    bool operator<(const LogOff & rhs) const
    {
        if (wrap_ < rhs.wrap_) return true;
        else if (wrap_ == rhs.wrap_) return off_ < rhs.off_;
        else return false;
    }

    uint32_t wrap_;
    uint64_t off_;
};

std::ostream & operator<<(std::ostream & os, const LogOff & off);

// ........................................................... LogAllocator ....

/**
 * @class LogAllocator
 *
 * Management offset allocation for log structured extent fs
 */
class LogAllocator
{
public:

    LogAllocator(const size_t pagesize, const disksize_t npages,
                 const diskoff_t startoff, const diskoff_t endoff)
        : log_("/extfs/logalloc")
        , pagesize_(pagesize)
        , npages_(npages)
        , startoff_(startoff)
        , endoff_(endoff)
    {
        INFO(log_) << "LogAllocator :"
                   << " pagesize: " << pagesize_
                   << " npages: " << npages_
                   << " startoff: " << startoff_
                   << " endoff: " << endoff_;
    }

    void SetOff(const LogOff & off)
    {
        off_ = off;
    }

    LogOff Next(const size_t size, LogOff & prev);

    void Inc(LogOff & off, const size_t size)
    {
        off.off_ += size;

        if (off.off_ > endoff_) {
            off.wrap_ += off.off_ / endoff_;
            off.off_ = startoff_ + off.off_ % endoff_;
        }
    }

private:

    LogPath log_;
    const size_t pagesize_;
    const disksize_t npages_;
    const diskoff_t startoff_;
    const diskoff_t endoff_;
    LogOff off_;
};

} // namespace extentfs

#endif /* _EXTENTFS_LOG_ALLOCATOR_H_ */
