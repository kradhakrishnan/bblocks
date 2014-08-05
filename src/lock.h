#ifndef _CORE_LOCK_H_
#define _CORE_LOCK_H_

#include <inttypes.h>

#include "perf/perf-counter.h"
#include "logger.h"

namespace bblocks {

#define ENTER_CRITICAL_SECTION(x) { AutoLock _(&x);
#define LEAVE_CRITICAL_SECTION }

class WaitCondition;

class Mutex
{
public:

    virtual void Lock() = 0;
    virtual void Unlock() = 0;

    virtual bool IsOwner() = 0;

    virtual ~Mutex() {}
};

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

    AutoLock()
        : mutex_(NULL)
    {
    }

    Mutex * mutex_;

private:

    AutoLock(AutoLock &);
};

using Guard = AutoLock;

class AutoUnlock : public AutoLock
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

class PThreadMutex : public Mutex
{
public:

    friend class WaitCondition;

    PThreadMutex(bool isRecursive = true)
        : isRecursive_(isRecursive)
    {
        pthread_mutexattr_t attr;
        int status;
        status = pthread_mutexattr_init(&attr);
        (void) status;
        ASSERT(status == 0);

        // Enable/disable recursive locking
        if (isRecursive_) {
            status = pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
            ASSERT(status == 0);
        }

#ifdef ERROR_CHECK
        // Enable error checking
        status = pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_ERRORCHECK);
        ASSERT(status == 0);
#endif
        status = pthread_mutex_init(&mutex_, &attr);
        ASSERT(status == 0);
    }

    bool TryLock()
    {
        return pthread_mutex_trylock(&mutex_) == 0;
    }

    virtual void Lock() override
    {
        int status = pthread_mutex_lock(&mutex_);
        (void) status;
        ASSERT(status == 0);
    }

    virtual void Unlock() override
    {
        int status = pthread_mutex_unlock(&mutex_);
        (void) status;
        ASSERT(status == 0);
    }

    virtual bool IsOwner() override
    {
        return pthread_mutex_lock(&mutex_) == EDEADLK;
    }

    virtual ~PThreadMutex()
    {
        pthread_mutex_destroy(&mutex_);
        // ASSERT(status == 0);
    }

    private:

    const bool isRecursive_;
    pthread_mutex_t mutex_;
};

class WaitCondition
{
public:

    WaitCondition()
    {
        int status = pthread_cond_init(&cond_, /*attr=*/ NULL);
        (void) status;
        ASSERT(status == 0);
    }

    ~WaitCondition()
    {
        int status = pthread_cond_destroy(&cond_);
        (void) status;
        // ASSERT(status == 0);
    }

    void Wait(PThreadMutex * lock)
    {
        int status = pthread_cond_wait(&cond_, &lock->mutex_);
        (void) status;
        ASSERT(status == 0);
    }

    void Signal()
    {
        int status = pthread_cond_signal(&cond_);
        (void) status;
        ASSERT(status == 0);
    }

    void Broadcast()
    {
        int status = pthread_cond_broadcast(&cond_);
        (void) status;
        ASSERT(status == 0);
    }

private:

    pthread_cond_t cond_;
};

class SpinMutex : public Mutex
{
public:

    enum
    {
        OPEN = 0x01,
        CLOSED = 0x11
    };

    explicit SpinMutex(const std::string & name)
        : name_("/spinmutex" + name)
        , mutex_(OPEN)
        , statSpinTime_(name_ + "/spin-time", "microsec", PerfCounter::TIME)
    {
        ASSERT(Is(OPEN));
    }

    ~SpinMutex()
    {
        VERBOSE(LogPath("/SpinMutex")) << statSpinTime_;
    }

    virtual void Lock()
    {
        INVARIANT(Is(OPEN) || !IsOwner());

        uint64_t start_ms = Time::NowInMilliSec();

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

        statSpinTime_.Update((Time::NowInMilliSec() - start_ms) * 1000);
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

    const std::string name_;
    pthread_t owner_;
    volatile _Atomic_word mutex_;

    PerfCounter statSpinTime_;
};

#define READ_LOCK(x) AutoReadLock _(x);
#define WRITE_LOCK(x) AutoWriteLock __(x);
#define READ_UNLOCK _.Unlock();
#define WRITE_UNLOCK __.Unlock();

class RWLock
{
public:

    virtual void ReadLock() = 0;
    virtual void WriteLock() = 0;
    virtual void Unlock() = 0;
};

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
