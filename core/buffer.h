#ifndef _DH_CORE_BUFFER_H_
#define _DH_CORE_BUFFER_H_

#include <core/assert.h>
#include <core/atomic.h>

namespace dh_core {

class IOBuffer;

//.................................................................. Ptr<T> ....

#if 0
/**
 *
 */
template<class T>
class Ptr
{
public:

    friend class IOBuffer;

    //.... pointer types that can be hosted ...//

    struct Data
    {
        virtual ~Data() {}

        virtual void Trash() = 0;
        virtual void Dalloc() = 0;
        virtual void Copy(const Data * rhs) = 0;
    };

    struct RawData : public Data
    {
        RawData(const size_t size)
            : t_(NULL)
        {
            ASSERT(size);
            t_ = new T[size];
        }

        ~RawData() {}

        virtual void Trash()
        {
            ASSERT(t_);
            delete[] t_;
            t_ = NULL;
        }

        virtual void Dalloc()
        {
            t_ = NULL;
        }

        virtual void Copy(const Data * rhs)
        {
            t_ = static_cast<const RawData *>(rhs)->t_;
        }

        T * t_;
    };

    struct SmartData : public Data
    {
        SmartData(const size_t size)
            : t_(NULL), count_(NULL)
        {
            ASSERT(!t_ && !count_);

            t_ = new T[size];
            count_ = new AtomicCounter(/*count=*/ 1);
        }

        virtual ~SmartData() { Dalloc(); }

        virtual void Trash() { Dalloc(); }

        virtual void Dalloc()
        {
            if (t_) {
                if (count_->Add(-1)) {
                    delete count_;
                    delete[] t_;
                }
            }

            t_ = NULL;
            count_ = NULL;
        }

        void Copy(const Data * rhs)
        {
            if (t_) {
                Dalloc();
            }

            count_->Add(/*count=*/ 1);

            ASSERT(!t_ && !count_)
            t_ = static_cast<SmartData *>(rhs)->t_;
            count_ = static_cast<SmartData *>(rhs)->count_;
        }

        T * t_;
        AtomicCounter * count_;
    };


    //.... static allocators ....//

    static Ptr<T> AllocRawPtr(const size_t size)
    {
        return Ptr<T>(new RawData(size));
    }

    static Ptr<T> AllocSmartPtr(const size_t size)
    {
        return Ptr<T>(new SmartData(size));
    }

    //.... Create/destroy ....//

    Ptr(const Ptr<T> & rhs)
    {
        data_->Copy(this, rhs);
    }

    Ptr<T> & operator=(const Ptr<T> & rhs)
    {
        data_->Copy(this, rhs);
        return *this;
    }

    ~Ptr()
    {
        if (data_) {
            data_->Dalloc(this);
            data_ = NULL;
        }
    }

    void Trash()
    {
        data_->Trash(this);
        data_ = NULL;
    }

    //.... typical ptr operations ....//

    T * operator->() const
    {
        ASSERT(data_);
        return data_;
    }

    operator bool()
    {
        return data_;
    }

private:

    Ptr(Data * data) : data_(data) {}

    Data * data_;
};
#endif

//................................................................. IOBuffer ...

/**
 *
 */
class IOBuffer
{
public:

    //.... static methods ....//

    static IOBuffer Alloc(const size_t size)
    {
        return IOBuffer(new uint8_t[size], size);
    }

    //.... create/destroy ....//

    IOBuffer() : data_(NULL), size_(0), off_(0) {}

    ~IOBuffer()
    {
    }

    //.... operators ....//

    uint8_t * operator->()
    {
        return data_ + off_;
    }

    operator bool() const
    {
        return data_;
    }

    //.... member fns ....//

    uint8_t * Ptr() { ASSERT(data_); return data_; }
    size_t Size() const { return size_; }

    void Reset()
    {
        data_ = NULL;
        size_ = off_ = 0;
    }

    void Trash()
    {
        ASSERT(data_);
        delete[] data_;

        Reset();
    }

    IOBuffer Cut(const size_t size)
    {
        ASSERT(size < size_);

        off_ += size;
        size_ -= size;

        return IOBuffer(data_, size, off_);
    }

    void FillRandom()
    {
        for (uint32_t i = 0; i < size_; ++i) {
            data_[off_ + i] = rand() % 255;
        }
    }

private:

    IOBuffer(uint8_t * data, const size_t size, const size_t off = 0)
        : data_(data), size_(size), off_(off)
    {
    }

    //.... member variables ....//

    uint8_t * data_;
    size_t size_;
    size_t off_;
};

}

#endif
