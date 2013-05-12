#ifndef _IOCORE_UTIL_H_
#define _IOCORE_UTIL_H_

#include <list>
#include <queue>
#include <sstream>
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/lexical_cast.hpp>
#include <inttypes.h>
#include <rpc/xdr.h>
#include <zlib.h>

#include "core/atomic.h"

namespace dh_core {

typedef int fd_t;
typedef uint32_t __u32;
typedef uint64_t __u64;
typedef int status_t;

enum
{
    OK = 0,
    FAIL = -1
};

#define SharedPtr boost::shared_ptr
#define SharedPtrBase boost::enable_shared_from_this
#define STR(x) boost::lexical_cast<std::string>(x)

#ifdef __GNUC__
#define likely(x)       __builtin_expect((x),1)
#define unlikely(x)     __builtin_expect((x),0)
#else
#define likely(x)       (x)
#define unlikely(x)     (x)
#endif

#if defined(__i386__)
static inline unsigned long long rdtsc(void)
{
  unsigned long long int x;
     __asm__ volatile (".byte 0x0f, 0x31" : "=A" (x));
     return x;
}
#elif defined(__x86_64__)
static inline unsigned long long rdtsc(void)
{
  unsigned hi, lo;
  __asm__ __volatile__ ("rdtsc" : "=a"(lo), "=d"(hi));
  return ( (unsigned long long)lo)|( ((unsigned long long)hi)<<32 );
}
#endif

template<class T>
SharedPtr<T>
MakeSharedPtr(T * t)
{
    return SharedPtr<T>(t);
}

/**
 */
class RefCounted
{
public:

    RefCounted()
        : refs_(1)
    {
    }

    void IncRef()
    {
        refs_.Add(/*count=*/ 1);
    }

    void DecRef()
    {
        const uint64_t count = refs_.Add(/*count=*/ -1);
        if (!count) {
            delete this;
        }
    }


protected:

    virtual ~RefCounted()
    {
    }

    AtomicCounter refs_;
};

template<class T>
class Ref
{
public:

    Ref(T * t) : t_(t) {}
    ~Ref()
    {
        if (t_) {
            t_->DecRef();
            t_ = NULL;
        }
    }

    Ref(Ref<T> & rhs)
    {
        ASSERT(!t_);
        rhs.t_->IncRef();
        t_ = rhs.t_;
    }

private:

    T * t_;
};

/**
 */
template<class T>
class AutoPtr
{
public:

    AutoPtr(T * t = NULL)
        : t_(t)
    {}

    ~AutoPtr()
    {
        Destroy();
    }

    inline T * Get() const
    {
        return t_;
    }

    inline T * operator->()
    {
        ASSERT(t_);
        return t_;
    }

    void Destroy()
    {
        if (!t_) {
            delete t_;
            t_ = NULL;
        }
    }

private:

    T * t_;
};

/**
 */
class RawData
{
public:

    RawData(const SharedPtr<uint8_t> data, const uint32_t off, const uint32_t size)
        : data_(data), off_(off), size_(size)
    {}

    RawData(const uint32_t size)
        : data_(new uint8_t[size]), off_(0), size_(size)
    {
        memset(data_.get(), 0xdd, size);
    }

    ~RawData()
    {
    }

    virtual uint8_t * Get(uint32_t off = 0) const
    {
        ASSERT(off < size_);
        return data_.get() + off_ + off;
    }

    virtual uint32_t Size() const { return size_; }

    virtual RawData Cut(uint32_t size)
    {
        ASSERT(data_);
        ASSERT(size_);
        ASSERT(size <= size_);

        RawData buf(data_, off_, size);

        off_ += size;
        size_ -= size;

        return buf;
    }

    void Set(uint8_t data, uint32_t off, uint32_t len)
    {
        ASSERT(off + len <= size_);
        memset(data_.get() + off, data, len);
    }

    void FillRandom()
    {
        for (uint32_t i = 0; i < size_; ++i) {
            data_.get()[off_ + i] = rand() % 255;
        }
    }

    std::string Dump() const
    {
        std::ostringstream ss;

        ss << "RawData:" << std::endl
           << "addr: " << (uint64_t) data_.get() << std::endl
           << "off: " << off_ << std::endl
           << "size: " << size_ << std::endl
           << "---- dump ----";

        const uint32_t CHARPERLINE = 32;

        std::ostringstream ascii;
        for (uint32_t i = 0; i < off_ + size_; ) {
            if (i && (i % CHARPERLINE)) {
                ss << ".";
            }

            uint8_t ch = data_.get()[i];
            ss << (uint32_t) ch;

            // adjust ascii printout
            if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z')
                || (ch >= '1' && ch <= '0')) {
                ascii << ch;
            } else {
                ascii << ".";
            }

            ++i;

            if(!(i % CHARPERLINE)) {
                ss << '\t' << ascii.str() << std::endl << i << '\t';
                ascii.str("");
            }
        }

        ss << std::endl << "----------------------" << std::endl;

        return ss.str();
    }


protected:

    RawData();

    SharedPtr<uint8_t> data_;
    uint32_t off_;
    uint32_t size_;
};


class DataBuffer
{
public:

    typedef std::list<RawData>::iterator iterator;

    DataBuffer()
        : size_(0)
    {
    }

    ~DataBuffer()
    {
    }

    void Append(const DataBuffer & buf)
    {
        buffer_.insert(buffer_.end(), buf.buffer_.begin(), buf.buffer_.end());
        size_ += buf.size_;
    }

    void Append(const RawData & data)
    {
        buffer_.push_back(data);
        size_ += data.Size();
    }

    bool IsEmpty() const
    {
        return buffer_.empty();
    }

    void Clear()
    {
        buffer_.clear();
        size_ = 0;
    }

    iterator Begin()
    {
        return buffer_.begin();
    }

    iterator End()
    {
        return buffer_.end();
    }

    size_t GetLength() const
    {
        return buffer_.size();
    }

    RawData & Front()
    {
        return buffer_.front();
    }

    void PopFront()
    {
        RawData & data = buffer_.front();
        ASSERT(size_ >= data.Size());
        size_ -= data.Size();

        buffer_.pop_front();
    }

    void CutFront(const uint32_t size)
    {
        buffer_.front().Cut(size);
        size_ -= size;
    }

    uint32_t Size() const
    {
        return size_;
    }

private:

    typedef std::list<RawData> datalist_t;

    uint32_t size_;
    datalist_t buffer_;
};

/**
 */
class Adler32
{
public:

    Adler32()
        : cksum_(0)
    {
    }

    void Update(uint8_t * data, const uint32_t size)
    {
        cksum_ = adler32(cksum_, data, size);
    }

    static uint32_t Calc(const RawData & data)
    {
        return adler32(/*cksum=*/ 0, data.Get(), data.Size());
    }

    static uint32_t Calc(uint8_t * data, const uint32_t size)
    {
        return adler32(/*cksum=*/ 0, data, size);
    }

    uint32_t Hash() const
    {
        return cksum_;
    }

    void Reset()
    {
        cksum_ = 0;
    }

private:

    uint32_t cksum_;
};


/**
 */
template<class T>
class StateMachine
{
    public:

    StateMachine(const T & state)
        : state_(state)
    {
    }

    bool MoveTo(const T & to, const unsigned int & from)
    {
        ASSERT(from);

        if (state_ & from) {
            state_ = to;
            return true;
        }

        return false;
    }

    T MoveTo(const T & to)
    {
        ASSERT(!state_);
        ASSERT(to);

        T old = state_;
        state_ = to;
        return old;
    }

    bool Is(const unsigned & states) const
    {
        ASSERT(states);
        return state_ & states;
    }

    bool operator==(const T & rhs)
    {
        return state_ == rhs;
    }

    const T & state() const
    {
        return state_;
    }

    private:

    T state_;
};

/**
 */
template<class T>
class Singleton
{
    public:

    static void Init()
    {
        ASSERT(!Singleton<T>::instance_);
        Singleton<T>::instance_ = new T();
    }

    static T & Instance()
    {
        ASSERT(Singleton<T>::instance_);
        return *instance_;
    }

    static void Destroy()
    {
        ASSERT(Singleton<T>::instance_);
        delete Singleton<T>::instance_;
        Singleton<T>::instance_ = NULL;
    }

    private:

    static T * instance_;
};

template<class T>
T * Singleton<T>::instance_ = NULL;

//............................................................ BoundedQueue ....

template<class T>
class BoundedQ
{
public:

    BoundedQ(const size_t capacity)
    {
        q_.reserve(capacity);
    }

    void Push(const T & t)
    {
        q_.push_back(t);
    }

    bool IsEmpty() const
    {
        return q_.empty();
    }

    size_t Size() const
    {
        return q_.size();
    }

    T Pop()
    {
        T t = q_.front();
        // pop front
        q_.erase(q_.begin());
        return t;
    }

    T & Front()
    {
        return q_.front();
    }

    typename std::vector<T>::iterator Begin()
    {
        return q_.begin();
    }

    typename std::vector<T>::iterator End()
    {
        return q_.end();
    }

    void Clear()
    {
        q_.clear();
    }

private:

    std::vector<T> q_;
};

};

#endif
