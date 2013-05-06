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

namespace dh_core {

class EpollSet;

/**
 *
 */
class EpollSetClient
{
public:

    virtual ~EpollSetClient() {}

    /**
     *
     */
    __sync__ virtual void EpollSetHandleFdEvent(int fd, uint32_t events) = 0;
};

/**
 */
class EpollWorker : public Thread
{
public:

    struct FdEvents
    {
        FdEvents(int fd, uint32_t events) : fd_(fd), events_(events) {}

        int fd_;
        uint32_t events_;
        EpollSetClient * client_;
    };

    EpollWorker(const std::string & name, EpollSet * epoll)
        : Thread(name), epoll_(epoll)
    {
    }

    virtual void * ThreadMain();

private:

    EpollSet * epoll_;
};

/**
 *
 */
class EpollSet : public Thread
{
public:

    friend class EpollWorker;

    // ctor and dtor
    EpollSet(const std::string & logPath);
    virtual ~EpollSet();

    // Add a fd for polling
    void Add(const fd_t fd, const uint32_t events, EpollSetClient * client,
             Callback<status_t> * cb = NULL);
    // Remove a fd from polling
    void Remove(const fd_t fd, Callback<status_t> * cb = NULL);
    // Add an event to an existing fd
    void AddEvent(const fd_t fd, const uint32_t events);
    // Stop polling for an event for a given fd
    void RemoveEvent(const fd_t fd, const uint32_t events);

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
    typedef std::tr1::unordered_map<fd_t, Callback<status_t> *> fdcb_map_t;
    typedef std::tr1::unordered_set<fd_t> fd_set_t;

    // Main loop of the thrread
    virtual void * ThreadMain();

    // Remove the fds from epoll
    void RemoveFd(const fd_t fd);
    // Remove all fds from the pendingRemove_ list
    void RemovePendingFds();

    void StopWorkers();

    LogPath log_;
    SpinMutex lock_;
    fd_t fd_;
    fd_map_t fdmap_;
    fd_set_t pendingAcks_;
    fdcb_map_t pendingRemove_;
    WaitCondition waitForAck_;
    std::vector<EpollWorker *> workers_;
    Queue<EpollWorker::FdEvents> workerq_;
    AtomicCounter pendingJobs_;
};

} // namespace kware {

#endif
