#ifndef _KASYNC_EPOLL_SET_H_
#define _KASYNC_EPOLL_SET_H_

#include <set>
#include <map>
#include <list>
#include <sys/epoll.h>
#include <stdint.h>
#include <tr1/unordered_set>
#include <tr1/unordered_map>

#include "core/util.hpp"
#include "core/thread.h"
#include "core/lock.h"
#include "core/callback.hpp"
#include "core/async.h"

namespace dh_core {

//................................................................ EpollSet ....

/**
 *
 */
class EpollSet : public Thread
{
public:

    typedef void (CHandle::*NotifyFn)(EpollSet *, int, uint32_t);

    //.... create/destroy ....//

    EpollSet(const std::string & logPath);
    virtual ~EpollSet();

    //.... sync operations ....//

    /*!
     *
     */
    bool Add(const fd_t fd, const uint32_t events, CHandle * chandle,
             NotifyFn fn);

    /*!
     *
     */
    bool Remove(const fd_t fd);

    /*!
     *
     */
    void AddEvent(const fd_t fd, const uint32_t events);

    /*!
     *
     */
    void RemoveEvent(const fd_t fd, const uint32_t events);

private:

    // Maximum epoll events that can polled for
    static const int MAX_EPOLL_EVENT = 1024;

    struct FDRecord
    {
        FDRecord() {}

        FDRecord(const uint32_t events, CHandle * chandle, NotifyFn fn)
            : events_(events), chandle_(chandle), fn_(fn)
        {}

        uint32_t events_;
        CHandle * chandle_;
        NotifyFn fn_;
    };

    typedef std::tr1::unordered_map<fd_t, FDRecord> fd_map_t;
    typedef std::tr1::unordered_map<fd_t, Callback<status_t> *> fdcb_map_t;
    typedef std::tr1::unordered_set<fd_t> fd_set_t;

    virtual void * ThreadMain();

    LogPath log_;
    SpinMutex lock_;
    fd_t fd_;
    fd_map_t fdmap_;
};

} // namespace kware {

#endif
