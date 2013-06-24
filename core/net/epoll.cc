#include "core/net/epoll.h"
#include "core/thread-pool.h"

using namespace std;
using namespace dh_core;

static void init_ee(epoll_event & ee, const int fd, const uint32_t events)
{
    memset(&ee, /*ch=*/ 0, sizeof(ee));
    ee.data.fd = fd;
    ee.events = events;
}

//................................................................ Epoll ....

//
// Create/destroy
//
Epoll::Epoll(const string & logPath)
    : Thread("Epoll/" + STR(this))
    , log_(logPath + "Epoll/")
{
    fd_ = epoll_create(/*size=*/ MAX_EPOLL_EVENT);

    if (fd_ == -1) {
        ERROR(log_) << "epoll_create failed. errno: " << errno;
    }

    INVARIANT(fd_ != -1);

    INFO(log_) << "epoll_create. fd=" << fd_;

    // Start polling
    StartBlockingThread();
}

Epoll::~Epoll()
{
    INFO(log_) << "Stopping epoll.";

    // Stop the polling thread
    Thread::Stop();
    // Close the poll fd
    ::close(fd_);

    {
        Guard _(&lock_);
        INVARIANT(fdmap_.empty());
    }
}

//
// Sync operations
//
bool
Epoll::Add(const fd_t fd, const uint32_t events, const chandler_t & chandler)
{
    ASSERT(!lock_.IsOwner());

    DEBUG(log_) << "Add. fd:" << fd << ", events:" << events;

    //
    // Insert to fdmap
    //
    {
        Guard _(&lock_);

        INVARIANT(fdmap_.find(fd) == fdmap_.end());
        fdmap_.insert(make_pair(fd, FDRecord(events, chandler)));
    }

    //
    // Notify the kernel
    //
    epoll_event ee;
    init_ee(ee, fd, events);

    int status = epoll_ctl(fd_, EPOLL_CTL_ADD, ee.data.fd, &ee);

    if (status == -1) {
        ERROR(log_) << "Error adding to epoll."
                    << " fd=" << fd << " events=" << events
                    << " errno: " << errno;
    }

    return (status != -1);
}

bool
Epoll::Remove(const fd_t fd)
{
    ASSERT(!lock_.IsOwner());

    DEBUG(log_) << "Remove. fd:" << fd;

    //
    // Remove from fdmap
    //
    {
        Guard _(&lock_);

        auto it = fdmap_.find(fd);
        INVARIANT(it != fdmap_.end());
        fdmap_.erase(it);
    }

    //
    // Notify the kernel
    //
    epoll_event ee;
    init_ee(ee, fd, /*events=*/ 0);

    int status = epoll_ctl(fd_, EPOLL_CTL_DEL, ee.data.fd, &ee);

    if (status == -1) {
        ERROR(log_) << "Error removing." << " fd: " << fd
                    << " errno: " << errno;
    }

    return (status != -1);
}

bool
Epoll::AddEvent(const fd_t fd, const uint32_t events)
{
    ASSERT(!lock_.IsOwner());
    ASSERT(events & (EPOLLIN | EPOLLOUT));

    DEBUG(log_) << "AddEvent. fd:" << fd << " events:" << events;

    //
    // Update fdmap
    //
    {
        Guard _(&lock_);

        auto it = fdmap_.find(fd);
        INVARIANT(it != fdmap_.end());
        FDRecord & fdrec = it->second;
        fdrec.events_ |= events;
    }

    //
    // Notify the kernel
    //
    epoll_event ee;
    init_ee(ee, fd, events);

    int status = epoll_ctl(fd_, EPOLL_CTL_MOD, fd, &ee);

    if (status == -1) {
        ERROR(log_) << "Error adding  event."
                    << " fd: " << fd << " events " << events
                    << " errno: " << errno;
    }

    return (status != -1);
}

bool
Epoll::RemoveEvent(const fd_t fd, const uint32_t events)
{
    ASSERT(!lock_.IsOwner());
    ASSERT(events & (EPOLLIN | EPOLLOUT));

    DEBUG(log_) << "RemoveEvent. fd=" << fd << " events=" << events;

    //
    // Update fdmap
    //
    {
        AutoLock _(&lock_);

        auto it = fdmap_.find(fd);
        INVARIANT(it != fdmap_.end());
        FDRecord & fdrec = it->second;
        fdrec.events_ &= ~events;
    }

    //
    // Notify the kernel
    //
    epoll_event ee;
    init_ee(ee, fd, events);

    int status = epoll_ctl(fd_, EPOLL_CTL_MOD, fd, &ee);

    if (status == -1) {
        ERROR(log_) << "Error removing event."
                    << " fd: " << fd << " events: " << events
                    << " errno: " << errno;
    }

    return (status != -1);
}

//
// Main loop
//
void *
Epoll::ThreadMain()
{
    vector<epoll_event> events;
    events.resize(MAX_EPOLL_EVENT);

    while (true) {
        int nfds = epoll_wait(fd_, &events[0], MAX_EPOLL_EVENT, /*ms=*/ -1);

        DEBUG(log_) << "Woke up. nfds=" << nfds;

        if (nfds == -1) {
            if (errno == EBADF) {
                // epoll is closed
                ERROR(log_) << "Epoll is closed.";
                break;
            }

            if (errno == EINTR) {
                // interrupted, retry
                continue;
            }

            // TODO: Handle more common error codes
        }

        DEFENSIVE_CHECK(nfds > 0);

        for (int i = 0; i < nfds; ++i) {
            uint32_t events_mask = events[i].events;
            int fd = events[i].data.fd;

            DEBUG(log_) << "Active fd. fd=" << fd
                        << " events=" << events_mask; 

            chandler_t chandler;

            {
                Guard _(&lock_);

                auto it = fdmap_.find(fd);
                if (it == fdmap_.end()) {
                    // This can happen because we first update the fd map and
                    // then notify the kernel
                    DEBUG(log_) << "Fd not found. fd=" << fd;
                    continue;
                }

                FDRecord & fdrecord = it->second;
                ASSERT(fdrecord.events_ && events_mask);

                chandler = fdrecord.chandler_;
            }

            chandler.Wakeup(fd, events_mask);
        }
    } // while (true) {

    return NULL;
}

