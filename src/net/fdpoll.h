#pragma once

#include "async.h"

namespace dh_core {

class FdPoll
{
public:

	typedef int fd_t;
	typedef Fn2<int, uint32_t> fn_t;

	/**
	 * Add a given file descriptor to the epoll list
	 *
	 * @param   fd          File descriptor
	 * @param   events      Events to listen on
	 * @param   fn		Completion handle to notify
	 * @return  true if successful else false
	 */
	virtual bool Add(const fd_t fd, const uint32_t events, const fn_t & fn) = 0;

	/**
	 * Remove given file descriptor from the epoll list
	 *
	 * @param   fd      File desctiptor
	 * @return  true if successful else false
	 */
	virtual bool Remove(const fd_t fd) = 0;

	/**
	 * Remove a given event from the registered fd
	 */
	virtual bool AddEvent(const fd_t fd, const uint32_t events) = 0;

	/**
	 * Unregister a given event from the registered events for a specific fd
	 */
	virtual bool RemoveEvent(const fd_t fd, const uint32_t events) = 0;
};

}
