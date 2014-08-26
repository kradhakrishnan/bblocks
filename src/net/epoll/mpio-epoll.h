#pragma once

#include <memory>
#include <map>

#include "net/fdpoll.h"
#include "net/epoll/epoll.h"

namespace bblocks {

class MultiPathEpoll : public FdPoll
{
public:

	using FdPoll::fd_t;
	using FdPoll::fn_t;

	MultiPathEpoll(const size_t npollth, const string & logpath = "mp-epoll/")
		: lock_(logpath)
		, log_(logpath)
	{
		for (size_t i = 0; i < npollth; ++i) {
			auto epoll = shared_ptr<Epoll>(new Epoll(logpath + STR(i)));
			epolls_.push_back(epoll);
		}
	}

	virtual ~MultiPathEpoll()
	{
		Guard _(&lock_);
		epolls_.clear();
	}

	virtual bool Add(const fd_t fd, const uint32_t events, const fn_t & fn) override
	{
		Guard _(&lock_);

		INVARIANT(fdmap_.find(fd) == fdmap_.end());

		const size_t id = ++idx_ % epolls_.size();

		INFO(log_) << id;

		fdmap_[fd] = id;
		return epolls_[id]->Add(fd, events, fn);
	}

	virtual bool Remove(const fd_t fd) override
	{
		Guard _(&lock_);

		auto it = fdmap_.find(fd);
		INVARIANT(it != fdmap_.end());

		const size_t id = it->second;
		fdmap_.erase(it);
		return epolls_[id]->Remove(fd);
	}

	virtual bool AddEvent(const fd_t fd, const uint32_t events) override
	{
		Guard _(&lock_);

		auto it = fdmap_.find(fd);
		INVARIANT(it != fdmap_.end());

		return epolls_[it->second]->AddEvent(fd, events);
	}

	virtual bool RemoveEvent(const fd_t fd, const uint32_t events) override
	{
		Guard _(&lock_);

		auto it = fdmap_.find(fd);
		INVARIANT(it != fdmap_.end());

		return epolls_[it->second]->RemoveEvent(fd, events);
	}

private:

	typedef map<fd_t, size_t> fdmap_t;

	SpinMutex lock_;
	string log_;
	atomic<size_t> idx_;
	vector<shared_ptr<Epoll> > epolls_;
	fdmap_t fdmap_;
};

}
