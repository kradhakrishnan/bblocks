#pragma once

#include "schd/thread-pool.h"

namespace bblocks {

// ................................................................................... BBlocks ....

class BBlocks
{
public:

	static void Start(const uint32_t ncores = SysConf::NumCores())
	{
		/*
		 * We need to init the current thread context to enable buffering to schedule
		 */
		ThreadCtx::Init(/*tinst=*/ NULL);

		NonBlockingThreadPool::Instance().Start(ncores);
	}

	static void Shutdown()
	{
		NonBlockingThreadPool::Instance().Shutdown();
		ThreadCtx::Cleanup();
	}

	static void Wait()
	{
		NonBlockingThreadPool::Instance().Wait();
	}

	static void Wakeup()
	{
		NonBlockingThreadPool::Instance().Wakeup();
	}

	#define TP_SCHEDULE(n)									\
	template<class _OBJ_, TDEF(T,n)>							\
	static void Schedule(_OBJ_ * obj, void (_OBJ_::*fn)(TENUM(T,n)), TPARAM(T,t,n))		\
	{											\
	    NonBlockingThreadPool::Instance().Schedule(obj, fn, TARG(t,n));			\
	}											\

	TP_SCHEDULE(1) // void Schedule<T1>(...)
	TP_SCHEDULE(2) // void Schedule<T1,T2>(...)
	TP_SCHEDULE(3) // void Schedule<T1,T2,T3>(...)
	TP_SCHEDULE(4) // void Schedule<T1,T2,T3,T4>(...)

	static void Schedule(ThreadRoutine * r)
	{
		NonBlockingThreadPool::Instance().Schedule(r);
	}

	#define TP_SCHEDULE_BARRIER(n)								\
	template<class _OBJ_, TDEF(T,n)>							\
	static void ScheduleBarrier(_OBJ_ * obj, void (_OBJ_::*fn)(TENUM(T,n)), TPARAM(T,t,n))	\
	{											\
		NonBlockingThreadPool::Instance().ScheduleBarrier(obj, fn, TARG(t,n));		\
	}											\

	TP_SCHEDULE_BARRIER(1) // void ScheduleBarrier<T1>(...)
	TP_SCHEDULE_BARRIER(2) // void ScheduleBarrier<T1,T2>(...)
	TP_SCHEDULE_BARRIER(3) // void ScheduleBarrier<T1,T2,T3>(...)
	TP_SCHEDULE_BARRIER(4) // void ScheduleBarrier<T1,T2,T3,T4>(...)

	static void ScheduleBarrier(ThreadRoutine * r)
	{
		NonBlockingThreadPool::Instance().ScheduleBarrier(r);
	}

	static bool ShouldYield()
	{
		return NonBlockingThreadPool::Instance().ShouldYield();
	}

	const size_t ncpu()
	{
		return NonBlockingThreadPool::Instance().ncpu();
	}
};


}

