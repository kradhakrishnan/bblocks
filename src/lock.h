#ifndef _CORE_LOCK_H_
#define _CORE_LOCK_H_

#include <inttypes.h>

#include "perf/perf-counter.h"
#include "logger.h"

namespace bblocks {

#define ENTER_CRITICAL_SECTION(x) { AutoLock _(&x);
#define LEAVE_CRITICAL_SECTION }

class WaitCondition;

// ...................................................................................... Mutex ....

class Mutex
{
public:

	virtual void Lock() = 0;

	virtual void Unlock() = 0;

	virtual bool IsOwner() = 0;

	virtual ~Mutex() {}
};

// ................................................................................... AutoLock ....

class AutoLock
{
public:

	explicit AutoLock(Mutex * mutex)
		: mutex_(mutex)
	{
		ASSERT(mutex_);
		mutex_->Lock();
	}

	void Unlock()
	{
		ASSERT(mutex_);
		mutex_->Unlock();
		mutex_ = NULL;
	}

	~AutoLock()
	{
		if (mutex_) {
			mutex_->Unlock();
		}
	}

protected:

	AutoLock();

	Mutex * mutex_;

private:

	AutoLock(AutoLock &);
};

using Guard = AutoLock;

// ................................................................................. AutoUnlock ....

class AutoUnlock : private AutoLock
{
public:

	explicit AutoUnlock(Mutex * mutex)
	{
		mutex_ = mutex;
		ASSERT(mutex_);
	}

private:

	AutoUnlock();
};

// ............................................................................... PThreadMutex ....

class PThreadMutex : public Mutex
{
public:

	friend class WaitCondition;

	PThreadMutex(const string & path, bool isRecursive = false)
		: path_(path)
		, isRecursive_(isRecursive)
		, statLockTime_(path_ + "/lock-time", "microsec", PerfCounter::TIME)
	{
		pthread_mutexattr_t attr;
		int status;
		status = pthread_mutexattr_init(&attr);
		INVARIANT(status == 0);

		// Enable/disable recursive locking
		if (isRecursive_) {
			status = pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
			INVARIANT(status == 0);
		}

	#ifdef ERROR_CHECK
		// Enable error checking
		status = pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_ERRORCHECK);
		INVARIANT(status == 0);
	#endif

		status = pthread_mutex_init(&mutex_, &attr);
		INVARIANT(status == 0);
	}

	virtual ~PThreadMutex()
	{
		int status = pthread_mutex_destroy(&mutex_);
		INVARIANT(status == 0);

		VERBOSE(path_) << statLockTime_;
	}

	bool TryLock()
	{
		int status = pthread_mutex_trylock(&mutex_);

		if (status == 0) {
			/*
			 * Acquired the lock
			 */
			owner_ = pthread_self();
			return true;
		}

		return false;
	}

	virtual void Lock() override
	{
		const uint64_t startInMicroSec = Time::NowInMicroSec();

		int status = pthread_mutex_lock(&mutex_);
		INVARIANT(status == 0);

		statLockTime_.Update(Time::NowInMicroSec() - startInMicroSec);

		owner_ = pthread_self();
	}

	virtual void Unlock() override
	{
		/*
		 * TODO:
		 * ASSERT that only owner can unlock, though that doesn't necessarily have to be
		 * true.
		 */

		int status = pthread_mutex_unlock(&mutex_);
		INVARIANT(status == 0);
	}

	virtual bool IsOwner() override
	{
		if (TryLock())
		{
			/*
			 * Lock is open, cannot be the owner
			 */
			Unlock();
			return false;
		}

		return pthread_self() == owner_;
	}

	private:

	const string path_;
	const bool isRecursive_;
	pthread_mutex_t mutex_;
	pthread_t owner_;

	PerfCounter statLockTime_;
};

// .............................................................................. WaitCondition ....

class WaitCondition
{
public:

	WaitCondition()
	{
		int status = pthread_cond_init(&cond_, /*attr=*/ NULL);
		INVARIANT(status == 0);
	}

	~WaitCondition()
	{
		int status = pthread_cond_destroy(&cond_);
		(void) status;
		// TODO: INVARIANT(status == 0);
	}

	void Wait(PThreadMutex * lock)
	{
		int status = pthread_cond_wait(&cond_, &lock->mutex_);
		INVARIANT(status == 0);
	}

	void Signal()
	{
		int status = pthread_cond_signal(&cond_);
		INVARIANT(status == 0);
	}

	void Broadcast()
	{
		int status = pthread_cond_broadcast(&cond_);
		INVARIANT(status == 0);
	}

private:

	pthread_cond_t cond_;
};

// ........................................................................... PThreadSpinMutex ....

class PThreadSpinMutex : public Mutex
{
public:

	PThreadSpinMutex(const string & path)
		: path_(path)
		, statSpinTime_(path_ + "/spin-time", "microsec", PerfCounter::TIME)
	{
		int status = pthread_spin_init(&lock_, PTHREAD_PROCESS_PRIVATE);
		INVARIANT(status == 0);
	}

	virtual ~PThreadSpinMutex()
	{
		int status = pthread_spin_destroy(&lock_);
		INVARIANT(status == 0);

		VERBOSE(path_) << statSpinTime_;
	}

	virtual void Lock() override
	{
		const uint64_t startInMicroSec = Time::NowInMicroSec();

		int status = pthread_spin_lock(&lock_);
		INVARIANT(status == 0);

		statSpinTime_.Update(Time::NowInMicroSec() - startInMicroSec);

		owner_ = pthread_self();
	}

	virtual void Unlock() override
	{
		ASSERT(IsOwner());

		int status = pthread_spin_unlock(&lock_);
		INVARIANT(status == 0);
	}

	virtual bool IsOwner() override
	{
		if (TryLock()) {
			/*
			 * Lock is open, cannot be the owner
			 */
			 Unlock();
			 return false;
		}

		return owner_ == pthread_self();
	}

private:

	bool TryLock()
	{
		int status = pthread_spin_trylock(&lock_);

		if (status == 0) {
			/*
			 * acquired the lock
			 */
			owner_ = pthread_self();
			return true;
		}

		return false;
	}

	string path_;
	pthread_t owner_;
	pthread_spinlock_t lock_;

	PerfCounter statSpinTime_;
};

// ............................................................................ CustomSpinMutex ....

class CustomSpinMutex : public Mutex
{
public:

    enum
    {
        OPEN = 0x01,
        CLOSED = 0x11
    };

    explicit CustomSpinMutex(const string & name)
        : name_("/spinmutex" + name)
        , mutex_(OPEN)
        , statSpinTime_(name_ + "/spin-time", "microsec", PerfCounter::TIME)
    {
        ASSERT(Is(OPEN));
    }

    ~CustomSpinMutex()
    {
        VERBOSE(string("/SpinMutex")) << statSpinTime_;
    }

    virtual void Lock()
    {
        INVARIANT(Is(OPEN) || !IsOwner());

        uint64_t startInMicroSec = Time::NowInMicroSec();

        bool status = false;
        while (true)
        {
            status = __sync_bool_compare_and_swap(&mutex_, OPEN, CLOSED);

            if (status) {
                ASSERT(Is(CLOSED));
                owner_ = pthread_self();
                break;
            }

            pthread_yield();
        }

        statSpinTime_.Update((Time::NowInMicroSec() - startInMicroSec));
    }

    virtual void Unlock()
    {
        ASSERT(IsOwner());
        owner_ = 0;
        bool status = __sync_bool_compare_and_swap(&mutex_, CLOSED, OPEN);
        (void) status;
        ASSERT(status);
    }

    const bool Is(const uint32_t & value)
    {
        return __sync_bool_compare_and_swap(&mutex_, value, value);
    }

    virtual bool IsOwner()
    {
        return Is(CLOSED) && pthread_equal(owner_, pthread_self());
    }

protected:

    const string name_;
    pthread_t owner_;
    volatile _Atomic_word mutex_;

    PerfCounter statSpinTime_;
};

#ifdef DISABLE_SPINNING
using SpinMutex = PThreadMutex;
#else
using SpinMutex = PThreadSpinMutex;
#endif

// ...................................................................................... RWLock ...

class RWLock
{
public:

    virtual void ReadLock() = 0;
    virtual void WriteLock() = 0;
    virtual void Unlock() = 0;
};

// .............................................................................. PThreadRWLock ....

class PThreadRWLock : public RWLock
{
public:

    PThreadRWLock()
    {
        int status = pthread_rwlock_init(&rwlock_,  /*attr=*/ NULL);
        (void) status;
        ASSERT(status == 0);
    }

    ~PThreadRWLock()
    {
        int status = pthread_rwlock_destroy(&rwlock_);
        (void) status;
        ASSERT(status == 0);
    }

    void ReadLock()
    {
        int status = pthread_rwlock_rdlock(&rwlock_);
        (void) status;
        ASSERT(status == 0);
    }

    void Unlock()
    {
        int status = pthread_rwlock_unlock(&rwlock_);
        (void) status;
        ASSERT(status == 0);
    }

    void WriteLock()
    {
        int status = pthread_rwlock_rdlock(&rwlock_);
        (void) status;
        ASSERT(status == 0);
    }

protected:

    pthread_rwlock_t rwlock_;
};

// ............................................................................... AutoReadLock ....

class AutoReadLock
{
public:

    explicit AutoReadLock(RWLock * rwlock)
        : rwlock_(rwlock)
    {
        ASSERT(rwlock_);
        rwlock_->ReadLock();
    }

    void Unlock()
    {
        ASSERT(rwlock_);
        rwlock_->Unlock();
        rwlock_ = NULL;
    }

    ~AutoReadLock()
    {
        if (rwlock_) {
            rwlock_->Unlock();
        }
    }

private:

    AutoReadLock();

    RWLock * rwlock_;
};

// .............................................................................. AutoWriteLock ....

class AutoWriteLock
{
public:

    explicit AutoWriteLock(RWLock * rwlock)
        : rwlock_(rwlock)
    {
        ASSERT(rwlock_);
        rwlock_->WriteLock();
    }

    void Unlock()
    {
        ASSERT(rwlock_);
        rwlock_->Unlock();
        rwlock_ = NULL;
    }

    ~AutoWriteLock()
    {
        if (rwlock_) {
            rwlock_->Unlock();
        }
    }

private:

    AutoWriteLock();

    RWLock * rwlock_;
};

}

#endif
