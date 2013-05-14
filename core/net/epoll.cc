#include "core/net/epoll.h"
#include "core/thread-pool.h"

using namespace std;
using namespace dh_core;

//................................................................ Epoll ....

//
// Create/destroy
//

Epoll::Epoll(const string & logPath)
    : Thread("Epoll/" + STR(this))
    , log_(logPath + "Epoll/")
{
    fd_ = epoll_create(/*size=*/ MAX_EPOLL_EVENT);
    ASSERT(fd_ != -1);

    INFO(log_) << "epoll_create. fd=" << fd_;

    StartBlockingThread();
}

Epoll::~Epoll()
{
    INFO(log_) << "Stopping epoll.";

    Thread::Stop();
    ::close(fd_);

    AutoLock _(&lock_);

    INVARIANT(fdmap_.empty());
}

//
// Sync operations
//

bool
Epoll::Add(const fd_t fd, const uint32_t events, CHandle * chandle,
              NotifyFn fn)
{
    ASSERT(!lock_.IsOwner());

    DEBUG(log_) << "Add. fd:" << fd << ", events:" << events
                << " , chandle: " << hex << chandle;

    ENTER_CRITICAL_SECTION(lock_);

    // insert to fdmap
    INVARIANT(fdmap_.find(fd) == fdmap_.end());
    fdmap_.insert(make_pair(fd, FDRecord(events, chandle, fn)));

    LEAVE_CRITICAL_SECTION

    epoll_event ee;
    memset(&ee, 0, sizeof(epoll_event));
    ee.data.fd = fd;
    ee.events = events;

    int status = epoll_ctl(fd_, EPOLL_CTL_ADD, ee.data.fd, &ee);
    return (status != -1);
}

bool
Epoll::Remove(const fd_t fd)
{
    ASSERT(!lock_.IsOwner());

    DEBUG(log_) << "Remove. fd:" << fd;

    // remove from fdmap
    {
        AutoLock _(&lock_);

        fd_map_t::iterator it = fdmap_.find(fd);
        INVARIANT(it != fdmap_.end());
        fdmap_.erase(it);
    }

    epoll_event ee;
    memset(&ee, 0, sizeof(epoll_event));
    ee.data.fd = fd;
    ee.events = 0;

    int status = epoll_ctl(fd_, EPOLL_CTL_DEL, ee.data.fd, &ee);
    return (status != -1);
}

void
Epoll::AddEvent(const fd_t fd, const uint32_t events)
{
    ASSERT(!lock_.IsOwner());

    DEBUG(log_) << "AddEvent. fd:" << fd << " events:" << events;

    ASSERT(events & (EPOLLIN | EPOLLOUT));

    // update fdmap
    {
        AutoLock _(&lock_);

        fd_map_t::iterator it = fdmap_.find(fd);
        INVARIANT(it != fdmap_.end());
        FDRecord & fdrec = it->second;
        fdrec.events_ |= events;
    }

    epoll_event ee;
    memset(&ee, 0, sizeof(epoll_event));
    ee.data.fd = fd;
    ee.events = events;

    int status = epoll_ctl(fd_, EPOLL_CTL_MOD, fd, &ee);
    INVARIANT(status != -1);
}

void
Epoll::RemoveEvent(const fd_t fd, const uint32_t events)
{
    ASSERT(!lock_.IsOwner());

    DEBUG(log_) << "RemoveEvent. fd=" << fd << " events=" << events;

    // update the fdmap
    {
        AutoLock _(&lock_);

        ASSERT(events & (EPOLLIN | EPOLLOUT));
        fd_map_t::iterator it = fdmap_.find(fd);
        INVARIANT(it != fdmap_.end());
        FDRecord & fdrec = it->second;
        fdrec.events_ &= ~events;
    }

    epoll_event ee;
    memset(&ee, 0, sizeof(epoll_event));
    ee.data.fd = fd;
    ee.events = events;

    int status = epoll_ctl(fd_, EPOLL_CTL_MOD, fd, &ee);
    INVARIANT(status != -1);
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

        ASSERT(nfds > 0);
        ASSERT(!lock_.IsOwner());

        DEBUG(log_) << "Woken from sleep. nfds=" << nfds;

        for (int i = 0; i < nfds; ++i) {
            uint32_t events_mask = events[i].events;
            int fd = events[i].data.fd;

            DEBUG(log_) << "Active fd. fd=" << fd
                        << " events=" << events_mask; 

            // Fetch the callback to be invoked
            CHandle * chandle = NULL;
            NotifyFn fn = NULL;

            {
                AutoLock _(&lock_);

                fd_map_t::iterator it = fdmap_.find(fd);
                if (it == fdmap_.end()) {
                    ERROR(log_) << "Fd not found. fd=" << fd;
                    // NOTREACHED
                    continue;
                }

                FDRecord & fdrecord = it->second;
                ASSERT(fdrecord.events_ && events_mask);

                chandle = fdrecord.chandle_;
                fn = fdrecord.fn_;
            }

            (chandle->*fn)(this, fd, events_mask);
        }
    } // while (true) {

    return NULL;
}

