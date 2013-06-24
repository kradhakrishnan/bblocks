#ifndef _DH_CORE_BUFFER_H_
#define _DH_CORE_BUFFER_H_

#include <malloc.h>
#include <boost/shared_ptr.hpp>

#include "core/assert.h"
#include "core/atomic.h"

namespace dh_core {

class IOBuffer;

//................................................................. IOBuffer ...

/**
 *
 */
class IOBuffer
{
public:

    struct Dalloc
    {
        void operator()(uint8_t * data)
        {
            ::free(data);
        }
    };

    //.... static methods ....//

    static IOBuffer Alloc(const size_t size)
    {
        void * ptr;
        int status = posix_memalign(&ptr, 512, size);
        INVARIANT(status != -1);
        return IOBuffer(boost::shared_ptr<uint8_t>((uint8_t *) ptr, Dalloc()),
                        size);
    }

    //.... create/destroy ....//

    IOBuffer() : size_(0), off_(0) {}

    ~IOBuffer()
    {
    }

    //.... operators ....//

    uint8_t * operator->()
    {
        return data_.get() + off_;
    }

    operator bool() const
    {
        return data_.get();
    }

    //.... member fns ....//

    uint8_t * Ptr()
    {
        ASSERT(data_.get());
        return data_.get();
    }

    size_t Size() const
    {
        return size_;
    }

    void Reset()
    {
        data_.reset();
        size_ = off_ = 0;
    }

    void Trash()
    {
        data_.reset();
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
            data_.get()[off_ + i] = rand() % 255;
        }
    }

    void Fill(const uint8_t ch = 0)
    {
        memset(data_.get() + off_, ch, size_);
    }

    void Copy(uint8_t * src, const size_t size)
    {
        memcpy(data_.get(), src, size);
    }

    template<class T>
    void Copy(const T & t)
    {
        INVARIANT(sizeof(t) <= size_);
        memcpy(data_.get(), (uint8_t *) &t, sizeof(T));
    }

private:

    IOBuffer(const boost::shared_ptr<uint8_t> & data, const size_t size,
             const size_t off = 0)
        : data_(data), size_(size), off_(off)
    {
    }

    //.... member variables ....//

    boost::shared_ptr<uint8_t> data_;
    size_t size_;
    size_t off_;
};

}

#endif
