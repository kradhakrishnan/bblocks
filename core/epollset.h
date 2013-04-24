#ifndef _KASYNC_EPOLL_SET_H_
#define _KASYNC_EPOLL_SET_H_

#include <set>
#include <map>
#include <list>
#include <sys/epoll.h>
#include <stdint.h>
#include <tr1/unordered_set>
#include <tr1/unordered_map>

#include "core/thread.h"
#include "core/lock.h"

namespace dh_core {

class EpollSetClient
{
public:

    virtual void EpollSetHandleFdEvent(int fd, uint32_t events) = 0;
};

/**
 */
class EpollSet : public Thread
{
public:

    typedef int fd_t;

    // ctor and dtor
    EpollSet(const std::string & logPath);
    virtual ~EpollSet();

    // Add a fd for polling
    void Add(const fd_t fd, const uint32_t events, EpollSetClient * client);
    // Remove a fd from polling
    void Remove(const fd_t fd);
    // Add an event to an existing fd
    void AddEvent(const fd_t fd, const uint32_t events);
    // Stop polling for an event for a given fd
    void RemoveEvent(const fd_t fd, const uint32_t events);

    // Notification from fd that it has handled the event
    void NotifyHandled(fd_t fd);

private:

    // Maximum epoll events that can polled for
    static const int MAX_EPOLL_EVENT = 1024;

    /**
     */
    struct FDRecord
    {
        FDRecord() {}

        FDRecord(const uint32_t events, EpollSetClient * client)
            : events_(events), client_(client)
        {}

        uint32_t events_;
        EpollSetClient * client_;
    };

    typedef std::tr1::unordered_map<fd_t, FDRecord> fd_map_t;
    typedef std::tr1::unordered_set<fd_t> fd_set_t;

    // Main loop of the thrread
    virtual void * ThreadMain();

    LogPath log_;
    PThreadMutex lock_;
    fd_t fd_;
    fd_map_t fdmap_;
    fd_set_t pendingAcks_;
    WaitCondition waitForAck_;
};

} // namespace kware {

#endif
