#include "core/epollset.h"
#include "core/thread-pool.h"

using namespace std;
using namespace dh_core;

#define MAXNWORKERS 2

//............................................................. EpollWorker ....

void *
EpollWorker::ThreadMain()
{
    while (!exitMain_)
    {
        EnableThreadCancellation();
        FdEvents job = epoll_->workerq_.Pop();
        DisableThreadCancellation();

        job.client_->EpollSetHandleFdEvent(job.fd_, job.events_);
        epoll_->pendingJobs_.Add(-1);
    }

    return NULL;
}

//................................................................ EpollSet ....

EpollSet::EpollSet(const string & logPath)
    : Thread("epollset/" + STR(this))
    , log_(logPath + "epollset/")
    , workerq_("/epollset/workerq")
{
    fd_ = epoll_create(/*size=*/ MAX_EPOLL_EVENT);
    ASSERT(fd_ != -1);

    INFO(log_) << "epoll_create. fd=" << fd_;

    for (unsigned int i = 0; i < MAXNWORKERS; ++i) {
        EpollWorker * w = new EpollWorker("/epoll/worker/" + STR(i), this);
        workers_.push_back(w);
        w->StartBlockingThread();
    }

    StartBlockingThread();
}

void
EpollSet::StopWorkers()
{
    for (auto it = workers_.begin(); it != workers_.end(); ++it) {
        EpollWorker * w = *it;
        w->Stop();
        delete w;
    }

    workers_.clear();
}

void
EpollSet::Add(const fd_t fd, const uint32_t events, EpollSetClient * client,
              Callback<status_t> * cb)
{
    DEBUG(log_) << "Add. fd:" << fd << ", events:" << events
                << " , client: " << client;

    epoll_event ee;
#ifdef VALGRIND_BUILD
    memset(&ee, 0, sizeof(epoll_event));
#endif
    ee.data.fd = fd;
    ee.events = events;

    {
        AutoLock _(&lock_);

        INVARIANT(fdmap_.find(fd) == fdmap_.end());
        fdmap_.insert(make_pair(fd, FDRecord(events, client)));
    }

    int status = epoll_ctl(fd_, EPOLL_CTL_ADD, ee.data.fd, &ee);
    INVARIANT(status != -1);

    if (cb) cb->ScheduleCallback(OK);
}

void
EpollSet::Remove(const fd_t fd, Callback<status_t> * cb)
{
    AutoLock _(&lock_);

    DEBUG(log_) << "Remove. fd:" << fd;

    epoll_event ee;
#ifdef VALGRIND_BUILD
    memset(&ee, 0, sizeof(epoll_event));
#endif
    ee.data.fd = fd;
    ee.events = 0;

    // remove the fd from the epoll tracking
    int status = epoll_ctl(fd_, EPOLL_CTL_DEL, ee.data.fd, &ee);
    INVARIANT(status != -1);

    // remove the fd from fdmap
    fd_map_t::iterator it = fdmap_.find(fd);
    INVARIANT(it != fdmap_.end());
    fdmap_.erase(it);

    // notify the callback
    if (cb) cb->ScheduleCallback(OK);
}

void
EpollSet::RemoveFd(const fd_t fd)
{
}

void
EpollSet::RemovePendingFds()
{
    AutoLock _(&lock_);

    ASSERT(pendingAcks_.empty());

    for (fdcb_map_t::iterator it = pendingRemove_.begin();
         it != pendingRemove_.end(); ++it) {
        RemoveFd(it->first);
        if (it->second) it->second->ScheduleCallback(OK);
    }

    pendingRemove_.clear();
};

void
EpollSet::AddEvent(const fd_t fd, const uint32_t events)
{
    DEBUG(log_) << "AddEvent. fd:" << fd << " events:" << events;

    ASSERT(events & (EPOLLIN | EPOLLOUT));

    {
        AutoLock _(&lock_);

        fd_map_t::iterator it = fdmap_.find(fd);
        ASSERT(it != fdmap_.end());
        FDRecord & fdrec = it->second;
        fdrec.events_ |= events;
    }

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
    DEBUG(log_) << "RemoveEvent. fd=" << fd << " events=" << events;

    {
        AutoLock _(&lock_);

        ASSERT(events & (EPOLLIN | EPOLLOUT));
        fd_map_t::iterator it = fdmap_.find(fd);
        ASSERT(it != fdmap_.end());
        FDRecord & fdrec = it->second;
        fdrec.events_ &= ~events;
    }

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
    StopWorkers();

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

        for (int i = 0; i < nfds; ++i) {
            uint32_t events_mask = events[i].events;
            int fd = events[i].data.fd;

            DEBUG(log_) << "Active fd. fd=" << fd
                        << " events=" << events_mask; 

            // Fetch the callback to be invoked
            EpollSetClient * client = NULL;

            {

                AutoLock _(&lock_);
                fd_map_t::iterator it = fdmap_.find(fd);
                if (it == fdmap_.end()) {
                    ERROR(log_) << "Fd not found. fd=" << fd;
                    NOTREACHED
                    continue;
                }

                FDRecord & fdrecord = it->second;
                ASSERT(fdrecord.events_ && events_mask);

                client = fdrecord.client_;
            }

            client->EpollSetHandleFdEvent(fd, events_mask);
        }
    } // while (true) {

    return NULL;
}

