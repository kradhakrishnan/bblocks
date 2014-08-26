#pragma once

#include <sys/timerfd.h>
#include <stdexcept>
#include <set>

#include "buf/bufpool.h"
#include "schd/thread.h"

namespace bblocks {

class NonBlockingThread;

//............................................................................... ThreadRoutine ....

class ThreadRoutine : public InListElement<ThreadRoutine>
{
public:

	virtual void Run() = 0;
	virtual ~ThreadRoutine() {}
};

//................................................................................... FnPtr*<*> ....

#define FNPTR(n)										\
template<TDEF(T,n)>             								\
class FnPtr##n : public ThreadRoutine,	        						\
		 public BufferPoolObject<FnPtr##n<TENUM(T,n)> >					\
{												\
public:												\
												\
	FnPtr##n(void (*fn)(TENUM(T,n)), TPARAM(T,t,n))						\
		: fn_(fn), TASSIGN(t,n)								\
	{}											\
												\
	virtual void Run()									\
	{											\
		(*fn_)(TARGEX(t,_,n));							        \
		delete this;									\
	}											\
												\
private:											\
												\
	void (*fn_)(TENUM(T,n));								\
	TMEMBERDEF(T,t,n);									\
};												\

FNPTR(1)  // FnPtr1<T1>
FNPTR(2)  // FnPtr2<T1, T2>
FNPTR(3)  // FnPtr3<T1, T2, T3>
FNPTR(4)  // FnPtr4<T1, T2, T3, T4>

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

	NonBlockingThread(const string & path, const uint32_t id)
		: Thread(path)
		, exitMain_(false)
		, q_(path)
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

	class ThreadExitException : public runtime_error
	{
	public:

		ThreadExitException(const string & error)
		    : runtime_error(error)
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
};

// ................................................................................ TimeKeeper ....

class TimeKeeper : public Thread
{
public:

	using This = TimeKeeper;

	TimeKeeper(const string & path)
		: Thread(path + string("/thread"))
		, path_(path)
		, lock_(path_)
		, fd_(-1)
	{}

	~TimeKeeper()
	{
		INVARIANT(fd_ == -1);
		INVARIANT(timers_.empty());
	}

	bool Init()
	{
		INVARIANT(fd_ == -1);
		INVARIANT(timers_.empty());
		INVARIANT(!ThreadCtx::tinst_);

		/*
		 * Create timer
		 */
		fd_ = timerfd_create(CLOCK_MONOTONIC, /*flags=*/ 0);

		if (fd_ == -1) {
			ERROR(path_) << "Unable to create timer";
			return false;
		}

		Thread::StartBlockingThread();

		INFO(path_) << "Created time keeper successfully";

		return true;
	}

	bool Shutdown()
	{
		Guard _(&lock_);

		Thread::Cancel();
	    	Thread::Stop();

		/*
		 * We have to destroy the thread so BBlocks can be restarted
		 */
		Thread::Destroy();

		INVARIANT(fd_ > 0);
		close(fd_);
		fd_ = -1;

		/*
		 * Since ThreadRoutine is opaque, we cannot assume anything about its construction
		 * We demand that user clean up all timer events before stopping
		 */
		INVARIANT(timers_.empty());

		return true;
	}

	bool ScheduleIn(const uint32_t msec, ThreadRoutine * r)
	{
		Guard _(&lock_);

		timers_.insert(TimerEvent(GetTimeSpec(msec), r));

		return SetTimer();
	}


private:

	timespec GetTimeSpec(const uint32_t msec)
	{
		timespec t;

		int status = clock_gettime(CLOCK_MONOTONIC, &t);

		INVARIANT(status != -1);

		t.tv_sec += MSEC_TO_SEC(msec);
		t.tv_nsec += MSEC_TO_NSEC(msec % 1000);

		/*
		 * Max value for nsec is 999,999,999
		 * Adjust the values accordingly
		 */
		t.tv_sec += t.tv_nsec / 1000000000;
		t.tv_nsec = t.tv_nsec % 1000000000;

		ASSERT(t.tv_nsec <= 999999999);

		return t;
	}

	bool SetTimer()
	{
		ASSERT(lock_.IsOwner());
		INVARIANT(!timers_.empty());

		const timespec time = timers_.begin()->time_;

		itimerspec t;

		t.it_value.tv_sec = time.tv_sec;
		t.it_value.tv_nsec = time.tv_nsec;
		t.it_interval.tv_sec = t.it_interval.tv_nsec = 0;

		DEBUG(path_) << "Resetting timer to "
			     << time.tv_sec << "." << time.tv_nsec;

		int status = timerfd_settime(fd_, /*flags=*/ TFD_TIMER_ABSTIME, &t,
					     /*old-value=*/ NULL);

		if (status == -1) {
			ERROR(path_) << "Error setting timer. " << strerror(errno);
			return false;
		}

		return true;
	}

	virtual void * ThreadMain() override;

	struct TimerEvent
	{
		TimerEvent(const timespec time, ThreadRoutine * r)
			: time_(time)
			, r_(r)
		{
			INVARIANT(r_);
		}

		bool operator<(const TimerEvent & rhs)
		{
			return time_.tv_sec == rhs.time_.tv_sec ? time_.tv_nsec < rhs.time_.tv_nsec
							        : time_.tv_sec < rhs.time_.tv_sec;
		}

		timespec time_;
		ThreadRoutine * r_;
	};

	typedef multiset<TimerEvent> timer_set_t;

	const string path_;
	SpinMutex lock_;
	int fd_;
	timer_set_t timers_;
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
		: lock_("/NBTP/lock")
		, waitLock_("/NBTP/wait")
		, nextTh_(0)
		, timekeeper_("/NBTP/time-keeper")
	{}

	void Start(const uint32_t ncpu)
	{
		INVARIANT(ncpu <= SysConf::NumCores());

		Guard _(&lock_);

		//
		// Start timer
		//
		bool status = timekeeper_.Init();

		if (!status) {
			ERROR("/NBTP") << "Unable to start timekeeper." << strerror(errno);
			DEADEND
		}

		//
		// Start the threads
		//
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
		timekeeper_.Shutdown();
		DestroyThreads();
	}

	void Wakeup()
	{
		Guard _(&waitLock_);
		condExit_.Broadcast();
	}

	void Wait()
	{
		Guard _(&waitLock_);
		condExit_.Wait(&waitLock_);
	}

	#define NBTP_SCHEDULE(n)								\
	template<class _OBJ_, TDEF(T,n)>							\
	void Schedule(_OBJ_ * obj, void (_OBJ_::*fn)(TENUM(T,n)),				\
	              TPARAM(T,t,n))								\
	{											\
		ThreadRoutine * r;								\
		void * buf = BufferPool::Alloc<MemberFnPtr##n<_OBJ_, TENUM(T,n)> >();		\
		r = new (buf) MemberFnPtr##n<_OBJ_, TENUM(T,n)>(obj, fn, TARG(t,n));		\
		Guard _(&lock_);								\
		threads_[nextTh_++ % threads_.size()]->Push(r);					\
	}											\
												\
	template<TDEF(T,n)>									\
	void Schedule(void (*fn)(TENUM(T,n)), TPARAM(T,t,n))					\
	{											\
		ThreadRoutine * r;								\
		void * buf = BufferPool::Alloc<FnPtr##n<TENUM(T,n)> >();			\
		r = new (buf) FnPtr##n<TENUM(T,n)>(fn, TARG(t,n));				\
		Guard _(&lock_);								\
		threads_[nextTh_++ % threads_.size()]->Push(r);					\
	}											\
												\
	template<class _OBJ_, TDEF(T,n)>							\
	void ScheduleIn(const uint32_t ms, _OBJ_ * obj, void (_OBJ_::*fn)(TENUM(T,n)),		\
	                TPARAM(T,t,n))								\
	{											\
		ThreadRoutine * r;								\
		void * buf = BufferPool::Alloc<MemberFnPtr##n<_OBJ_, TENUM(T,n)> >();		\
		r = new (buf) MemberFnPtr##n<_OBJ_, TENUM(T,n)>(obj, fn, TARG(t,n));		\
		INVARIANT(timekeeper_.ScheduleIn(ms, r));					\
	}											\
												\
	template<TDEF(T,n)>									\
	void ScheduleIn(const uint32_t ms, void (*fn)(TENUM(T,n)), TPARAM(T,t,n))		\
	{											\
		ThreadRoutine * r;								\
		void * buf = BufferPool::Alloc<FnPtr##n<TENUM(T,n)> >();			\
		r = new (buf) FnPtr##n<TENUM(T,n)>(fn, TARG(t,n));				\
		INVARIANT(timekeeper_.ScheduleIn(ms, r));					\
	}											\


	NBTP_SCHEDULE(1) // void Schedule<T1>(...)
	NBTP_SCHEDULE(2) // void Schedule<T1,T2>(...)
	NBTP_SCHEDULE(3) // void Schedule<T1,T2,T3>(...)
	NBTP_SCHEDULE(4) // void Schedule<T1,T2,T3,T4>(...)

	void Schedule(ThreadRoutine * r)
	{
		Guard _(&lock_);
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
		Guard _(&lock_);								\
		threads_[nextTh_++ % threads_.size()]->Push(r);					\
	}											\

	NBTP_SCHEDULE_BARRIER(1) // void ScheduleBarrier<T1>(...)
	NBTP_SCHEDULE_BARRIER(2) // void ScheduleBarrier<T1,T2>(...)
	NBTP_SCHEDULE_BARRIER(3) // void ScheduleBarrier<T1,T2,T3>(...)
	NBTP_SCHEDULE_BARRIER(4) // void ScheduleBarrier<T1,T2,T3,T4>(...)

	void ScheduleBarrier(ThreadRoutine * r)
	{
		Guard _(&lock_);

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

	typedef vector<NonBlockingThread *> threads_t;

	void DestroyThreads()
	{
		Guard _(&lock_);

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


	SpinMutex lock_;
	PThreadMutex waitLock_;
	threads_t threads_;
	WaitCondition condExit_;
	uint32_t nextTh_;
	TimeKeeper timekeeper_;
};

} // namespace bblocks

