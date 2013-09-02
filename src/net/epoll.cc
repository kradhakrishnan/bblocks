#include <list>

#include "net/epoll.h"
#include "thread-pool.h"

using namespace std;
using namespace dh_core;

//....................................................................................... Epoll ....

Epoll::Epoll(const string & logPath)
    : Thread("/epoll/" + STR(this))
    , log_(logPath + "/epoll")
    , lock_("/epoll" + logPath)
{
	fd_ = epoll_create(/*size=*/ MAX_EPOLL_EVENT);

	if (fd_ == -1) {
		ERROR(log_) << "epoll_create failed. errno: " << errno;
	}

	INVARIANT(fd_ != -1);

	INFO(log_) << "epoll_create. fd=" << fd_;

	/*
	 * Start polling
	 */
	StartBlockingThread();
}

Epoll::~Epoll()
{
	INFO(log_) << "Stopping epoll.";

	/*
	 * Stop the polling thread
	 */
	Thread::Stop();

	/*
	 * Close fd
	 */
	::close(fd_);

	{
		/*
		 * Empty trash can (fds marked for deletion)
		 */
		Guard _(&lock_);
		INVARIANT(fdmap_.empty());
		EmptyTrashcan();
	}
}

bool
Epoll::Add(const fd_t fd, const uint32_t events, const fn_t & chandler)
{
	ASSERT(!lock_.IsOwner());

	DEBUG(log_) << "Add. fd:" << fd << ", events:" << events;

	FDRecord * fdrec = new FDRecord(fd, events, chandler);
	INVARIANT(fdrec);

	{
		/*
		 * Insert to fdmap
		 */
		Guard _(&lock_);
		INVARIANT(fdmap_.find(fd) == fdmap_.end());
		fdmap_.insert(make_pair(fd, fdrec));
	}

	/*
	 * Notify the kernel
	 */
	epoll_event ee = fdrec->GetEpollEvent();

	int status = epoll_ctl(fd_, EPOLL_CTL_ADD, fd, &ee);

	if (status == -1) {
		ERROR(log_) << "Error adding to epoll."
			    << " fd=" << fd << " events=" << events
			    << " errno: " << errno;

		/*
		 * It is safe to trash fdrec since the epoll add failed
		 */
		delete fdrec;
		return false;
	}

	return true;
}

bool
Epoll::Remove(const fd_t fd)
{
	ASSERT(!lock_.IsOwner());

	DEBUG(log_) << "Remove. fd:" << fd;

	FDRecord * fdrec = NULL;

	{
		/*
		 * Remove from fdmap
		 */
		Guard _(&lock_);

		auto it = fdmap_.find(fd);
		INVARIANT(it != fdmap_.end() && it->second);
		fdrec = it->second;
		fdmap_.erase(it);

		/*
		 * mute callbacks
		 */
		INVARIANT(!fdrec->mute_);
		fdrec->mute_ = true;
	}

	/*
	 * Notify the kernel
	 */
	int status = epoll_ctl(fd_, EPOLL_CTL_DEL, fd, /*ee=*/ NULL);

	if (status == -1) {
		ERROR(log_) << "Error removing." << " fd: " << fd
			    << " errno: " << errno;
		return false;
	}

	{
		/*
		 * Push into trash can. We don't delete the fd record here because we have
		 * tagged them as completion token with the epoll. We safely remove after
		 * being woken up by the epoll.
		 */
		Guard _(&lock_);
		trashcan_.push_back(fdrec);
	}

	return true;
}

bool
Epoll::AddEvent(const fd_t fd, const uint32_t events)
{
	ASSERT(!lock_.IsOwner());
	ASSERT(events & (EPOLLIN | EPOLLOUT));

	DEBUG(log_) << "AddEvent. fd:" << fd << " events:" << events;

	FDRecord * fdrec = NULL;

	{
		/*
		 * Update fdmap
		 */
		Guard _(&lock_);
		auto it = fdmap_.find(fd);
		INVARIANT(it != fdmap_.end() && it->second);
		fdrec = it->second;
		fdrec->events_ |= events;
	}

	/*
	 * Notify the kernel
	 */
	epoll_event ee = fdrec->GetEpollEvent();

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

	FDRecord * fdrec = NULL;

	{
		/*
		 * Update fdmap
		 */
		Guard _(&lock_);
		auto it = fdmap_.find(fd);
		INVARIANT(it != fdmap_.end() && it->second);
		fdrec = it->second;
		fdrec->events_ &= ~events;
	}

	/*
	 * Notify the kernel
	 */
	epoll_event ee = fdrec->GetEpollEvent();

	int status = epoll_ctl(fd_, EPOLL_CTL_MOD, fd, &ee);

	if (status == -1) {
		ERROR(log_) << "Error removing event."
			    << " fd: " << fd << " events: " << events
			    << " errno: " << errno;
	}

	return (status != -1);
}

void Epoll::EmptyTrashcan()
{
	INVARIANT(lock_.IsOwner());

	for (auto fdrec : trashcan_) {
		INVARIANT(fdrec->mute_);
		delete fdrec;
	}

	trashcan_.clear();
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
				/*
				 * epoll is closed
				 */
				ERROR(log_) << "Epoll is closed.";
				break;
			}

			if (errno == EINTR) {
				/*
				 * interrupted, retry
				 */
				continue;
			}

			// TODO: Handle more common error codes
			// We assume this is fatal
			DEADEND
		}

		DEFENSIVE_CHECK(nfds > 0);

		/*
		 * Wakeup completion handlers
		 */
		for (int i = 0; i < nfds; ++i) {
			uint32_t events_mask = events[i].events;
			FDRecord * fdrec = (FDRecord *) events[i].data.ptr;
			INVARIANT(fdrec);

			/*
			 * Ignore callback if muted
			 */
			{
				Guard _(&lock_);
				if (fdrec->mute_) continue;
			}

			DEBUG(log_) << "Active fd. fd=" << fdrec->fd_
				    << " events=" << fdrec->events_; 

			fdrec->fn_.Wakeup(fdrec->fd_, events_mask);
		}

		/*
		 * Take out the trash
		 */
		Guard _(&lock_);
		EmptyTrashcan();
	}

	return NULL;
}

