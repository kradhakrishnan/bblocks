#pragma once

#include <map>
#include <sys/epoll.h>
#include <stdint.h>
#include <tr1/unordered_map>

#include "net/fdpoll.h"
#include "util.hpp"
#include "lock.h"
#include "async.h"
#include "schd/thread.h"

namespace bblocks {

//....................................................................................... Epoll ....

/**
 * @class Epoll
 *
 * Provides an epoll wrapper functionality for polling socket and related file
 * descriptor asynchronously.
 *
 * Effectively the processing throughput will be what one can extract from a
 * single core.
 */
class Epoll : public Thread, public FdPoll
{
public:

	using FdPoll::fn_t;

	Epoll(const std::string & logPath);
	virtual ~Epoll();

	virtual bool Add(const fd_t fd, const uint32_t events, const fn_t & fn) override;
	virtual bool Remove(const fd_t fd) override;
	virtual bool AddEvent(const fd_t fd, const uint32_t events) override;
	virtual bool RemoveEvent(const fd_t fd, const uint32_t events) override;

private:

	static const int MAX_EPOLL_EVENT = 10 * 1024; // C10K

	/**
	 *  Represent the Fd being polled on and its related information
	 */
	struct FDRecord
	{
		FDRecord(const fd_t fd, const uint32_t events, const fn_t & fn)
			: fd_(fd), events_(events), fn_(fn), mute_(false)
		{}

		epoll_event GetEpollEvent()
		{
			epoll_event ee;
			memset(&ee, /*ch=*/ 0, sizeof(ee));
			ee.data.ptr = this;
			ee.events = events_;
			return ee;
		}

		fd_t fd_;               // Registered file descriptor
		uint32_t events_;       // Registered events
		fn_t fn_;		// Completion handler
		bool mute_;             // Don't invoke handler
	};

	typedef std::tr1::unordered_map<fd_t, FDRecord *> fd_map_t;
	typedef std::list<FDRecord *> fdrec_list_t;

	/**
	 * Epoll thread entry method
	 */
	virtual void * ThreadMain();

	/**
	 * Clear the fds that are marked for deletion
	 */
	void EmptyTrashcan();

	LogPath log_;		    // Log file
	SpinMutex lock_;	    // Default lock
	fd_t fd_;		    // Epoll fd
	fd_map_t fdmap_;	    // fd <-> FDRecord map
	fdrec_list_t trashcan_;	    // FDRecords to be trashed
};

}
