#ifndef _DHCORE_LOFF_H_
#define _DHCORE_LOFF_H_

#include "core/defs.h"

namespace dh_core {

/*!
 * /class   LogOff
 * /brief   Offset representation for log structure layout
 *
 */
struct LogOff
{
    static uint64_t noffs;

    LogOff() : wrap_(UINT32_MAX), off_(UINT64_MAX) {}

    void Inc(uint32_t offs)
    {
        off_ += offs;

        if (unlikely(off_ >= noffs)) {
            wrap_ += (off_ / noffs);
            off_ = off_ % noffs;
        }
    }

    std::ostream & operator<<(std::ostream & os)
    {
        os << STR("LogOff: wrap=") << wrap_ << " off=" << off_;
        return os;
    }

    uint32_t wrap_;
    uint64_t off_;
};

} // namespace dh_core

#endif /* _DHCORE_LOFF_H_ */
