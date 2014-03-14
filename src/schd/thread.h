#pragma once

#include <atomic>

#include "logger.h"
#include "util.hpp"
#include "schd/schd-helper.h"

namespace dh_core {

//...................................................................................... Thread ....

class Thread
{
public:

	friend class NonBlockingThreadPool;

	Thread(const std::string & logPath)
		: log_(logPath)
	{}

	void StartBlockingThread()
	{
		int ok = pthread_create(&tid_, /*attr=*/ NULL, ThFn, (void *)this);
		INVARIANT(!ok);

		INFO(log_) << "Thread " << tid_ << " created. (instance " << (long) this << ")";
	}

	void StartNonBlockingThread()
	{
		int ok = pthread_create(&tid_, /*attr=*/ NULL, ThFn, (void *)this);
		INVARIANT(!ok);

		INFO(log_) << "Thread " << tid_ << " created.";
	}

	virtual ~Thread()
	{
		INFO(log_) << "Thread " << tid_ << " destroyed.";
	}

	void EnableThreadCancellation()
	{
		int status = pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, /*oldstate=*/ NULL);
		(void) status;
		ASSERT(!status);
	}

	void DisableThreadCancellation()
	{
		int status = pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, /*oldstate=*/ NULL);
		(void) status;
		ASSERT(!status);
	}

	void Cancel()
	{
		int status = pthread_cancel(tid_);
		INVARIANT(!status);
	}

	void Detach()
	{
		int status = pthread_detach(tid_);
		INVARIANT(!status);
	}

	virtual void Stop()
	{
		void * ret;
		int status = pthread_join(tid_, &ret);
		INVARIANT(!status || ret == PTHREAD_CANCELED);
	}

	void SetProcessorAffinity()
	{
		const uint32_t core = RRCpuId::Instance().GetId();

		INFO(log_) << "Binding to core " << core;

		cpu_set_t cpuset;
		CPU_ZERO(&cpuset);
		CPU_SET(core, &cpuset);

		int status = pthread_setaffinity_np(tid_, sizeof(cpuset), &cpuset);
		INVARIANT(!status);
	}

	static void * ThFn(void * args)
	{
		INVARIANT(args);

		Thread * th = (Thread *) args;

		ThreadCtx::Init(th);

		void * thstatus = th->ThreadMain();

		ThreadCtx::Cleanup();

		return thstatus;
	}

protected:

	virtual bool ShouldYield()
	{
		/*
		 * This is a query used by async process to make sure they are not blocking other
		 * operations during long CPU intensive work.
		 */
		 return false;
	}

	virtual void * ThreadMain() = 0;

	LogPath log_;
	pthread_t tid_;
};

}
