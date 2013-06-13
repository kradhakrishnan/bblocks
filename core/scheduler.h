#ifndef _IOCORE_SCHEDULER_H_
#define _IOCORE_SCHEDULER_H_

#include <inttypes.h>

#include <pthread.h>
#include <boost/shared_ptr.hpp>
#include <signal.h>

#include "core/logger.h"
#include "core/inlist.hpp"

namespace dh_core {

class Scheduler;


/**
 *
 */
class SysConf
{
public:

    static uint32_t NumCores()
    {
        uint32_t numCores = sysconf(_SC_NPROCESSORS_ONLN);
        ASSERT(numCores >= 1);

        return numCores;
    }
};

/**
 *
 */
class RRCpuId : public Singleton<RRCpuId>
{
public:

    friend class Singleton<RRCpuId>;

    uint32_t GetId()
    {
        return nextId_++ % SysConf::NumCores();
    }

private:

    RRCpuId()
    {
        nextId_ = 0;
    }

    uint32_t nextId_;
};

} // namespace dh_core {

#endif
