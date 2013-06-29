#ifndef _DH_CORE_PERF_COUNTER_H_
#define _DH_CORE_PERF_COUNTER_H_

#include <string>
#include <ostream>
#include <atomic>

namespace dh_core {

///
///
///
class PerfCounter
{
public:

    /* ... ctor/dtor .... */

    PerfCounter(const std::string & name, const std::string & units)
        : name_(name)
        , units_(units)
        , val_(0)
        , count_(0)
        , min_(UINT32_MAX)
        , max_(0)
        , startms_(Time::NowInMilliSec())
    {
        InitBucket();
    }

    virtual ~PerfCounter() {}

    /* .... Public member functions .... */

    void Update(const uint32_t val)
    {
        val_.fetch_add(val);
        count_.fetch_add(/*val=*/ 1);

        uint64_t minCount = min_.load();
        while (val < minCount) {
            min_.compare_exchange_strong(minCount, val);
            minCount = min_.load();
        }

        uint64_t maxCount = max_.load();
        while (val > maxCount) {
            max_.compare_exchange_strong(maxCount, val);
            maxCount = max_.load();
        }

        UpdateBucket(val);
    }

    friend std::ostream & operator<<(std::ostream & os, const PerfCounter & pc)
    {
        os << "Perfcoutner: " << pc.name_ << std::endl;

        if (pc.count_.load()) {
            for (int i = 0; i < 32; ++i) {
                if (pc.bucket_[i].load()) {
                    os << " " << (i ? pow(2, i) : 0) << " - " << pow(2, i + 1)
                       << " : "
                       << pc.bucket_[i].load()
                       << std::endl;
                }
            }

            os << " Aggregate-value: " << pc.val_.load()
               << " Count: " << pc.count_.load()
               << " Time: " << pc.ElapsedSec() << " sec "
               << " Maximum: " << pc.max_.load() << " " << pc.units_
               << " Minimum: " << pc.min_.load() << " " << pc.units_
               << " Average: " << pc.Avg() << " " << pc.units_
               << " " << pc.units_ << "-per-sec : "
               << std::fixed << (pc.val_.load() / pc.ElapsedSec())
               << " " << "ops-per-sec : "
               << (pc.count_.load() / pc.ElapsedSec());
        }

        return os;
    }

protected:

    /* .... Protected member functions .... */

    double ElapsedSec() const
    {
        return (Time::NowInMilliSec() - startms_) / 1000;
    }

    double Avg() const
    {
        return count_.load() ? (val_.load() / (double) count_.load()) : 0;
    }

    void InitBucket()
    {
        for (int i = 0; i < 32; ++i) {
            bucket_[i].exchange(0);
        }
    }

    void UpdateBucket(const uint32_t val)
    {
        // the bucket contains value 2^idx - 2^(idx+1)
        // bucket 0 : 0 - 2
        // bucket 1 : 2 - 4
        // bucket 2 : 4 - 8
        // etc
        uint32_t tmp = val;
        uint32_t count = 0;
        for (int i = 0; i < 32; ++i) {
            tmp >>= 1;
            if (!tmp) break;
            ++count;
        }

        INVARIANT(count < 32);
        bucket_[count].fetch_add(/*val=*/ 1);
    }

    /* .... Private member variables .... */

    const std::string name_;
    const std::string units_;
    std::atomic<uint64_t> val_;
    std::atomic<uint64_t> count_;
    std::atomic<uint64_t> min_;
    std::atomic<uint64_t> max_;
    std::atomic<uint32_t> bucket_[32];
    uint64_t startms_;
};


} // namespace dh_core {

#endif // _DH_CORE_PERF_COUNTER_H_
