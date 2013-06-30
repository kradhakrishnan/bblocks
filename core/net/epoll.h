#ifndef _DH_CORE_EPOLL_H_
#define _DH_CORE_EPOLL_H_

#include <map>
#include <sys/epoll.h>
#include <stdint.h>
#include <tr1/unordered_map>

#include "core/util.hpp"
#include "core/thread.h"
#include "core/lock.h"
#include "core/async.h"

namespace dh_core {

//................................................................ Epoll ....

///
/// @class Epoll
///
/// Provides an epoll wrapper functionality for polling socket and related file
/// descriptor asynchronously.
///
/// Effectively the processing throughput will be what one can extract from a
/// single core.
///
class Epoll : public Thread
{
public:

    /* .... Typedef .... */

    typedef CHandler2<int, uint32_t> chandler_t;

    /* .... create/destroy .... */

    Epoll(const std::string & logPath);
    virtual ~Epoll();

    /* .... sync operations .... */

    ///
    /// Add a given file descriptor to the epoll list
    ///
    /// @param   fd          File descriptor
    /// @param   events      Events to listen on
    /// @param   chandler    Completion handle to notify
    /// @return  true if successful else false
    ///
    bool Add(const fd_t fd, const uint32_t events, const chandler_t & chandler);

    ///
    /// Remove given file descriptor from the epoll list
    ///
    /// @param   fd      File desctiptor
    /// @return  true if successful else false
    ///
    bool Remove(const fd_t fd);

    ///
    /// Remove a given event from the registered fd
    ///
    bool AddEvent(const fd_t fd, const uint32_t events);

    ///
    /// Unregister a given event from the registered events for a specific fd
    ///
    bool RemoveEvent(const fd_t fd, const uint32_t events);

private:

    /* .... Data .... */

    // Maximum epoll events that can polled for
    static const int MAX_EPOLL_EVENT = 1024;

    ///
    /// Represent the Fd being polled on and its related information
    ///
    struct FDRecord
    {
        FDRecord(const fd_t fd, const uint32_t events,
                 const chandler_t & chandler)
            : fd_(fd), events_(events), chandler_(chandler), mute_(false)
        {}

        epoll_event GetEpollEvent()
        {
            epoll_event ee;
            memset(&ee, /*ch=*/ 0, sizeof(ee));
            ee.data.ptr = this;
            ee.events = events_;
            return ee;
        }

        fd_t fd_;               // Registered file descriptor
        uint32_t events_;       // Registered events
        chandler_t chandler_;   // Completion handler
        bool mute_;             // Don't invoke handler
    };

    typedef std::tr1::unordered_map<fd_t, FDRecord *> fd_map_t;
    typedef std::list<FDRecord *> fdrec_list_t;

    /* .... Private methods ..... */

    ///
    /// Epoll thread entry method
    ///
    virtual void * ThreadMain();

    ///
    /// Clear the fds that are marked for deletion
    ///
    void EmptyTrashcan();

    /* .... Private member variables .... */

    LogPath log_;           // Log file
    SpinMutex lock_;        // Default lock
    fd_t fd_;               // Epoll fd
    fd_map_t fdmap_;        // fd <-> FDRecord map
    fdrec_list_t trashcan_; // FDRecords to be trashed
};

} // namespace kware {

#endif /* _DH_CORE_EPOLL_H_ */
