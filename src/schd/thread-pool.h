#ifndef _DH_CORE_THREADPOOL_H_
#define _DH_CORE_THREADPOOL_H_

#include <stdexcept>

#include "defs.h"
#include "buf/bufpool.h"
#include "schd/thread.h"

namespace dh_core {

class NonBlockingThread;

//............................................................................... ThreadRoutine ....

class ThreadRoutine : public InListElement<ThreadRoutine>
{
public:

	virtual void Run() = 0;
	virtual ~ThreadRoutine() {}
};

//............................................................................. MemberFnPtr*<*> ....

#define MEMBERFNPTR(n)										\
template<class _OBJ_, TDEF(T,n)>								\
class MemberFnPtr##n : public ThreadRoutine,							\
		       public BufferPoolObject<MemberFnPtr##n<_OBJ_, TENUM(T,n)> >		\
{												\
public:												\
												\
	MemberFnPtr##n(_OBJ_ * obj, void (_OBJ_::*fn)(TENUM(T,n)), TPARAM(T,t,n))		\
		: obj_(obj), fn_(fn), TASSIGN(t,n)						\
	{}											\
												\
	virtual void Run()									\
	{											\
		(obj_->*fn_)(TARGEX(t,_,n));							\
		delete this;									\
	}											\
												\
private:											\
												\
	_OBJ_ * obj_;										\
	void (_OBJ_::*fn_)(TENUM(T,n));								\
	TMEMBERDEF(T,t,n);									\
};												\

MEMBERFNPTR(1)  // MemberFnPtr1<_OBJ_, T1>
MEMBERFNPTR(2)  // MemberFnPtr2<_OBJ_, T1, T2>
MEMBERFNPTR(3)  // MemberFnPtr3<_OBJ_, T1, T2, T3>
MEMBERFNPTR(4)  // MemberFnPtr4<_OBJ_, T1, T2, T3, T4>

//........................................................................... NonBlockingThread ....

class NonBlockingThread : public Thread
{
public:

	NonBlockingThread(const std::string & path, const uint32_t id)
		: Thread(path)
		, exitMain_(false)
		, q_(path)
		, id_(id)
	{}

	virtual void * ThreadMain();

	void Push(ThreadRoutine * r)
	{
		q_.Push(r);
	}

	bool IsEmpty() const
	{
		return q_.IsEmpty();
	}

	virtual void Stop() override
	{
		INVARIANT(!exitMain_);
		exitMain_ = true;

		INVARIANT(q_.IsEmpty());

		/*
		 * Push a message so, we can wakeup the main thread and exit it
		 */
		Push(new ThreadExitRoutine());
    
		int status = pthread_join(tid_, NULL);
		INVARIANT(!status);
	}

private:

	class ThreadExitException : public std::runtime_error
	{
	public:

		ThreadExitException(const std::string & error)
		    : std::runtime_error(error)
		{}
	};

	struct ThreadExitRoutine : ThreadRoutine
	{
		virtual void Run()
		{
			delete this;

			/*
			 * The intention of scheduling this routine is to destroy the thread which
			 * is process the requests. Kill the thread.
			 */
			throw ThreadExitException("pthread_exit proxy");
		}
	};

	virtual bool ShouldYield() override
	{
		return !q_.IsEmpty();
	}

	bool exitMain_;
	InQueue<ThreadRoutine> q_;
	const uint32_t id_;
};

//....................................................................... NonBlockingThreadPool ....

class NonBlockingThreadPool : public Singleton<NonBlockingThreadPool>
{
public:

	friend class NonBlockingThread;

	class BarrierRoutine
	{
	public:

		BarrierRoutine(ThreadRoutine * cb, const size_t count)
			: cb_(cb), pendingCalls_(count)
		{}

		void Run(int)
		{ 
			const uint64_t count = pendingCalls_.Add(/*count=*/ -1);

			if (count == 1) {
				INVARIANT(!pendingCalls_.Count());
				NonBlockingThreadPool::Instance().Schedule(cb_);
				cb_ = NULL;
				delete this;
			}
		}

	private:

		ThreadRoutine * cb_;
		AtomicCounter pendingCalls_;
	};


	NonBlockingThreadPool()
		: nextTh_(0)
	{}

	void Start(const uint32_t ncpu)
	{
		INVARIANT(ncpu <= SysConf::NumCores());

		Guard _(&lock_);

		for (size_t i = 0; i < ncpu; ++i) {
			NonBlockingThread * th = new NonBlockingThread("/th/" + STR(i), i);
			threads_.push_back(th);
			th->StartNonBlockingThread();
		}
	}

	size_t ncpu() const
	{
		return threads_.size();
	}

	void Shutdown()
	{
		Guard _(&lock_);
		DestroyThreads();
	}

	void Wakeup()
	{
		Guard _(&lock_);
		condExit_.Broadcast();
	}

	void Wait()
	{
		Guard _(&lock_);
		condExit_.Wait(&lock_);
	}

	#define NBTP_SCHEDULE(n)								\
	template<class _OBJ_, TDEF(T,n)>							\
	void Schedule(_OBJ_ * obj, void (_OBJ_::*fn)(TENUM(T,n)),				\
	              TPARAM(T,t,n))								\
	{											\
		ThreadRoutine * r;								\
		void * buf = BufferPool::Alloc<MemberFnPtr##n<_OBJ_, TENUM(T,n)> >();		\
		r = new (buf) MemberFnPtr##n<_OBJ_, TENUM(T,n)>(obj, fn, TARG(t,n));		\
		threads_[nextTh_++ % threads_.size()]->Push(r);					\
	}											\

	NBTP_SCHEDULE(1) // void Schedule<T1>(...)
	NBTP_SCHEDULE(2) // void Schedule<T1,T2>(...)
	NBTP_SCHEDULE(3) // void Schedule<T1,T2,T3>(...)
	NBTP_SCHEDULE(4) // void Schedule<T1,T2,T3,T4>(...)

	void Schedule(ThreadRoutine * r)
	{
		threads_[nextTh_++ % threads_.size()]->Push(r);
	}

	bool ShouldYield();

	#define NBTP_SCHEDULE_BARRIER(n)							\
	template<class _OBJ_, TDEF(T,n)>							\
	void ScheduleBarrier(_OBJ_ * obj, void (_OBJ_::*fn)(TENUM(T,n)), TPARAM(T,t,n))		\
	{											\
		ThreadRoutine * r;								\
		void * buf = BufferPool::Alloc<MemberFnPtr##n<_OBJ_, TENUM(T,n)> >();		\
		r = new (buf) MemberFnPtr##n<_OBJ_, TENUM(T,n)>(obj, fn, TARG(t,n));	    	\
		threads_[nextTh_++ % threads_.size()]->Push(r);					\
	}											\

	NBTP_SCHEDULE_BARRIER(1) // void ScheduleBarrier<T1>(...)
	NBTP_SCHEDULE_BARRIER(2) // void ScheduleBarrier<T1,T2>(...)
	NBTP_SCHEDULE_BARRIER(3) // void ScheduleBarrier<T1,T2,T3>(...)
	NBTP_SCHEDULE_BARRIER(4) // void ScheduleBarrier<T1,T2,T3,T4>(...)

	void ScheduleBarrier(ThreadRoutine * r)
	{
		BarrierRoutine * br = new BarrierRoutine(r, threads_.size());
		for (size_t i = 0; i < threads_.size(); ++i) {
			ThreadRoutine * r;
			void * buf = BufferPool::Alloc<MemberFnPtr1<BarrierRoutine, int> >(); 
			r = new (buf) MemberFnPtr1<BarrierRoutine, int>(br, &BarrierRoutine::Run,
									/*status=*/ 0);
			threads_[i]->Push(r);
		}
	}

private:

	typedef std::vector<NonBlockingThread *> threads_t;

	void DestroyThreads()
	{
		for (auto it = threads_.begin(); it != threads_.end(); ++it) {
			NonBlockingThread * th = *it;
			/*
			 * Stop and destroy the thread
			 */
			th->Stop();
			delete th;
		}

		threads_.clear();
	}


	PThreadMutex lock_;
	threads_t threads_;
	WaitCondition condExit_;
	uint32_t nextTh_;
};

// ................................................................................. ThreadPool ....

class ThreadPool
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


} // namespace dh_core

#endif
