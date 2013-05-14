#ifndef _DH_CORE_EPOLL_H_
#define _DH_CORE_EPOLL_H_

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

//................................................................ Epoll ....

/*!
 * \class Epoll
 * \brief Provides an epoll wrapper functionality for polling socket and
 *        related file descriptor asynchronously.
 *
 * We are following the pro-actor design pattern for asynchronous IO processing.
 * Epoll service is an anomaly to the pattern, it is actually a blocking service
 * which interrupts the completion handler synchronously when event are detected
 * on the given fd.
 *
 * Effectively the processing throughput will be what one can extract from a
 * single core. We need to added more delegation of events across cores to go
 * further. So, far experimentation show that one core on an average box is good
 * for around 2Gbps with a single connection.
 *
 */
class Epoll : public Thread
{
public:

    typedef void (CHandle::*NotifyFn)(Epoll *, int, uint32_t);

    //.... create/destroy ....//

    Epoll(const std::string & logPath);
    virtual ~Epoll();

    //.... sync operations ....//

    /*!
     . Add a given file descriptor to the epoll list
     .
     . \param   fd      File descriptor
     . \param   events  Events to listen on
     . \param   chandle Completion handle to notify
     . \param   fn      Method to notify under the completion handle
     . \return  true if successful else false
     */
    bool Add(const fd_t fd, const uint32_t events, CHandle * chandle,
             NotifyFn fn);

    /*!
     . Remove given file descriptor from the epoll list
     .
     . \param   fd      File desctiptor
     . \return  true if successful else false
     */
    bool Remove(const fd_t fd);

    /*!
     * \brief Remove a given event from the registered fd
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

    /*! \struct FDRecrod
     *  \brief  Represent the Fd being polled on and its related information
     */
    struct FDRecord
    {
        FDRecord() : chandle_(NULL) {}
        FDRecord(const uint32_t events, CHandle * chandle, NotifyFn fn)
            : events_(events), chandle_(chandle), fn_(fn)
        {}

        uint32_t events_;
        CHandle * chandle_;
        NotifyFn fn_;
    };

    typedef std::tr1::unordered_map<fd_t, FDRecord> fd_map_t;

    virtual void * ThreadMain();

    LogPath log_;
    SpinMutex lock_;
    fd_t fd_;
    fd_map_t fdmap_;
};

} // namespace kware {

#endif /* _DH_CORE_EPOLL_H_ */
