#pragma once

#include "defs.h"
#include "schd/thread-pool.h"

namespace bblocks {

class ThreadRoutine;

// ................................................................................... BBlocks ....

class BBlocks
{
public:

	static void Start(const uint32_t ncores);
        static void Start();

	static void Shutdown();

	static void Wait();

	static void Wakeup();

	#define TP_SCHEDULE(n)									\
	template<class _OBJ_, TDEF(T,n)>							\
	static void Schedule(_OBJ_ * obj, void (_OBJ_::*fn)(TENUM(T,n)), TPARAM(T,t,n))		\
	{											\
		NonBlockingThreadPool::Instance().Schedule(obj, fn, TARG(t,n));			\
	}											\
												\
	template<class _OBJ_, TDEF(T,n)>							\
	static void ScheduleIn(const uint32_t msec, _OBJ_ * obj, void (_OBJ_::*fn)(TENUM(T,n)), \
			       TPARAM(T,t,n))							\
	{											\
		NonBlockingThreadPool::Instance().ScheduleIn(msec, obj, fn, TARG(t,n));		\
	}											\


	TP_SCHEDULE(1) // void Schedule<T1>(...)
	TP_SCHEDULE(2) // void Schedule<T1,T2>(...)
	TP_SCHEDULE(3) // void Schedule<T1,T2,T3>(...)
	TP_SCHEDULE(4) // void Schedule<T1,T2,T3,T4>(...)

	static void Schedule(ThreadRoutine * r);

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

	static void ScheduleBarrier(ThreadRoutine * r);

	static bool ShouldYield();

	static const size_t ncpu();
};


}

