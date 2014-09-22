#pragma once

#include <string>
#include <ostream>
#include <atomic>
#include <iomanip>
#include <map>

namespace bblocks {

// ................................................................................ PerfCounter ....

/**
 * @class General purpose performance counter implementation
 *
 * PerfCounter is general purpose implementation and can be used to capture stats as a counter of
 * items, bytes or time. It also includes a bucket stat which can capture data distribution
 * statistics.
 */
class PerfCounter
{
public:

	enum Type
	{
		COUNTER = 0,
		BYTES,
		TIME,
	};

	PerfCounter(const string & name, const string & units, const Type & type)
		: name_(name)
		, units_(units)
		, type_(type)
		, val_(0)
		, count_(0)
		, min_(UINT32_MAX)
		, max_(0)
		, startms_(Rdtsc::NowInMilliSec())
	{
		InitBucket();
	}

	virtual ~PerfCounter() {}

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

	friend ostream & operator<<(ostream & os, const PerfCounter & pc)
	{
		os << "Perfcoutner: " << pc.name_ << endl;

		if (!pc.count_) return os;

		kvs_t kv;

		kv["Aggregate-value"] = STR(pc.val_.load());
		kv["Count"] = STR(pc.count_.load());
		kv["Time"] = STR(pc.ElapsedSec()) + " s";
		kv["Max"] = STR(pc.max_.load()) + " " + pc.units_;
		kv["Min"] = STR(pc.min_.load()) + " " + pc.units_;
		kv["Avg"] = STR(to_h(pc.Avg())) + " " + pc.units_;

		if (pc.type_ == BYTES) {
			kv[pc.units_ + "-per-sec"] = STR(to_h(pc.val_.load() / pc.ElapsedSec()));
			kv["ops-per-sec"] = STR(to_h((pc.count_.load() / pc.ElapsedSec())));
		}

		Print(os, kv);

		kv.clear();
		if (pc.count_.load()) {
			for (int i = 0; i < 32; ++i) {
				if (!pc.bucket_[i].load()) continue;

				auto k = to_h(i ? pow(2, i) : 0) + "-" + to_h(pow(2, i + 1));
				kv[k] = to_h(pc.bucket_[i].load());
			}
		}

		Print(os, kv);
		return os;
	}

protected:

	typedef map<string, string> kvs_t;

	static void DrawLine(ostream & os)
	{
		os << "+" << setfill('-') << setw(30) << "-"
		   << "+" << setfill('-') << setw(30) << "-"
		   << "+" << endl << setfill(' ');
	}

	static void Print(ostream & os, const kvs_t & kvs)
	{
		DrawLine(os);

		for (auto kv : kvs)
		{
			os << "|" << setw(30) << left << kv.first << "|"
			   << setw(30) << left << kv.second << "|"
			   << endl;
		}

		DrawLine(os);
	}

	double ElapsedSec() const
	{
		return Rdtsc::ElapsedInMilliSec(startms_) / 1000;
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

	template<class T>
	static const string to_h(const T & val)
	{
		if (val >= 1000 * 1000) {
			return STR(int(val / (1000 * 1000))) + "M";
		} else if (val >= 1000) {
			return STR(int(val / 1000)) + "K";
		} else {
			return STR(val);
		}
	}

	const string name_;
	const string units_;
	const Type type_;
	atomic<uint64_t> val_;
	atomic<uint64_t> count_;
	atomic<uint64_t> min_;
	atomic<uint64_t> max_;
	atomic<uint32_t> bucket_[32];
	uint64_t startms_;
};

// ............................................................................. TimeCounter<T> ....

/**
 * @class TimeCounter<T>
 *
 * General purpose time keeper. It can be used to track time take by individual pieces of code
 * marked by the enum passed as T.
 */
template<class T>
class TimeCounter
{
public:

	TimeCounter(const string & name)
		: name_(name)
		, startms_(Rdtsc::NowInMilliSec())
		, refms_(Rdtsc::NowInMilliSec())
	{
		for (int i = 0; i < sizeof(timer_); ++i) {
			timer_[i].store(/*val=*/ 0);
		}
	}

	void ClockIn(const T & t)
	{
		INVARIANT(t < 32);
		timer_[t] += Rdtsc::ElapsedInMilliSec(refms_);
		refms_ = Rdtsc::NowInMilliSec();
	}

	friend ostream & operator<<(ostream & os, const TimeCounter<T> & v)
	{
		const uint64_t elapsed_ms = Rdtsc::ElapsedInMilliSec(v.startms_);

		os << "TimeCounter : " << v.name_ << endl
		   << " Elapsed: " << elapsed_ms << " ms" << endl;

		for (int i = 0; i < 32; ++i) {
			if (!v.timer[i]) continue;

			os << T(i) << " " << v.timer_[i]
			   << " ms ( " << (v.timer_[i] * 100 / elapsed_ms) << "% )"
			   << endl;
		}

		return os;
	}

private:

	const string name_;
	const uint64_t startms_;
	uint64_t refms_;
	atomic<uint32_t> timer_[32];
};

}
