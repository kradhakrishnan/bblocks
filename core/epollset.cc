#include "core/epollset.h"
#include "core/thread-pool.h"

using namespace std;
using namespace dh_core;

EpollSet::EpollSet(const string & logPath)
    : Thread("epollset/" + STR(this))
    , log_(logPath + "epollset/")
{
    fd_ = epoll_create(/*size=*/ MAX_EPOLL_EVENT);
    ASSERT(fd_ != -1);

    INFO(log_) << "epoll_create. fd=" << fd_;

    StartThread();
}

void
EpollSet::NotifyHandled(fd_t fd)
{
    AutoLock _(&lock_);

    ASSERT(!pendingAcks_.empty());

    fd_set_t::iterator it = pendingAcks_.find(fd);
    ASSERT(it != pendingAcks_.end());
    pendingAcks_.erase(it);

    if (pendingAcks_.empty()) {
        DEBUG(log_) << "Waking up.";
        waitForAck_.Broadcast();
    }
}

void
EpollSet::Add(const fd_t fd, const uint32_t events, EpollSetClient * client)
{
    AutoLock _(&lock_);

    DEBUG(log_) << "Add. fd:" << fd << ", events:" << events
                << " , client: " << client;

    epoll_event ee;
#ifdef VALGRIND_BUILD
    memset(&ee, 0, sizeof(epoll_event));
#endif
    ee.data.fd = fd;
    ee.events = events;

    int status = epoll_ctl(fd_, EPOLL_CTL_ADD, ee.data.fd, &ee);
    INVARIANT(status != -1);

    INVARIANT(fdmap_.find(fd) == fdmap_.end());
    fdmap_.insert(make_pair(fd, FDRecord(events, client)));
}

void
EpollSet::Remove(const fd_t fd)
{
    AutoLock _(&lock_);

    DEBUG(log_) << "Remove. fd:" << fd;

    epoll_event ee;
#ifdef VALGRIND_BUILD
    memset(&ee, 0, sizeof(epoll_event));
#endif
    ee.data.fd = fd;
    ee.events = 0;

    int status = epoll_ctl(fd_, EPOLL_CTL_DEL, ee.data.fd, &ee);
    INVARIANT(status != -1);

    fd_map_t::iterator it = fdmap_.find(fd);
    INVARIANT(it != fdmap_.end());
    fdmap_.erase(it);
}

void
EpollSet::AddEvent(const fd_t fd, const uint32_t events)
{
    AutoLock _(&lock_);

    DEBUG(log_) << "AddEvent. fd:" << fd << " events:" << events;

    ASSERT(events & (EPOLLIN | EPOLLOUT));

    fd_map_t::iterator it = fdmap_.find(fd);
    ASSERT(it != fdmap_.end());
    FDRecord & fdrec = it->second;
    fdrec.events_ |= events;

    epoll_event ee;
#ifdef VALGRIND_BUILD
    memset(&ee, 0, sizeof(epoll_event));
#endif
    ee.data.fd = fd;
    ee.events = events;

    int status = epoll_ctl(fd_, EPOLL_CTL_MOD, fd, &ee);
    INVARIANT(status != -1);
}

void
EpollSet::RemoveEvent(const fd_t fd, const uint32_t events)
{
    AutoLock _(&lock_);

    DEBUG(log_) << "RemoveEvent. fd=" << fd << " events=" << events;

    ASSERT(events & (EPOLLIN | EPOLLOUT));
    fd_map_t::iterator it = fdmap_.find(fd);
    ASSERT(it != fdmap_.end());
    FDRecord & fdrec = it->second;
    fdrec.events_ &= ~events;

    epoll_event ee;
#ifdef VALGRIND_BUILD
    memset(&ee, 0, sizeof(epoll_event));
#endif
    ee.data.fd = fd;
    ee.events = events;

    // Update epoll
    int status = epoll_ctl(fd_, EPOLL_CTL_MOD, fd, &ee);
    INVARIANT(status != -1);
}

EpollSet::~EpollSet()
{
    INFO(log_) << "~. fd=" << fd_ << " fdmap=" << fdmap_.size();

    ASSERT(fdmap_.empty());

    ::close(fd_);
}

void *
EpollSet::ThreadMain()
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
        }

        ASSERT(nfds != -1);
        ASSERT(nfds > 0);

        DEBUG(log_) << "Woken from sleep. nfds=" << nfds;

        AutoLock _(&lock_);
        for (int i = 0; i < nfds; ++i) {
            uint32_t events_mask = events[i].events;
            int fd = events[i].data.fd;

            DEBUG(log_) << "Active fd. fd=" << fd
                        << " events=" << events_mask; 

            // Fetch the callback to be invoked
            fd_map_t::iterator it = fdmap_.find(fd);
            if (it == fdmap_.end()) {
                ERROR(log_) << "Fd not found. fd=" << fd;
                NOTREACHED
                continue;
            }

            FDRecord & fdrecord = it->second;
            ASSERT(fdrecord.events_ && events_mask);

            pendingAcks_.insert(fd);

            // make a callback
            NonBlockingThreadPool::Instance().Schedule(
                    fdrecord.client_, &EpollSetClient::EpollSetHandleFdEvent, 
                    fd, events_mask);
        }

        // Go into a wait
        DEBUG(log_) << "Waiting for ack.";
        ASSERT(!pendingAcks_.empty());
        // XXX adaptive spinning
        waitForAck_.Wait(&lock_);
        ASSERT(pendingAcks_.empty());
    } // while (true)

    return NULL;
}

