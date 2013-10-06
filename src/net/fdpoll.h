#pragma once

#include "async.h"

namespace dh_core {

class FdPoll
{
public:

	typedef int fd_t;
	typedef Fn2<int, uint32_t> fn_t;

	virtual bool Add(const fd_t fd, const uint32_t events, const fn_t & fn) = 0;
	virtual bool Remove(const fd_t fd) = 0;
	virtual bool AddEvent(const fd_t fd, const uint32_t events) = 0;
	virtual bool RemoveEvent(const fd_t fd, const uint32_t events) = 0;
};

}
