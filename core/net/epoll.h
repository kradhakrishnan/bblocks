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
        FDRecord(const uint32_t events, const chandler_t & chandler)
            : events_(events), chandler_(chandler)
        {}

        uint32_t events_;       // Registered events
        chandler_t chandler_;   // Completion handler
    };

    typedef std::tr1::unordered_map<fd_t, FDRecord> fd_map_t;

    /* .... Private methods ..... */

    ///
    /// Epoll thread entry method
    ///
    virtual void * ThreadMain();

    /* .... Private member variables .... */

    LogPath log_;       // Log file
    SpinMutex lock_;    // Default lock
    fd_t fd_;           // Epoll fd
    fd_map_t fdmap_;    // fd <-> FDRecord map
};

} // namespace kware {

#endif /* _DH_CORE_EPOLL_H_ */
