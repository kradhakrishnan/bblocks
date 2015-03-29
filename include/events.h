#pragma once

#include <string>
#include <list>

#include "defs.h"
#include "util.h"
#include "lock.h"
#include "schd/thread.h"

namespace bblocks
{

// ...................................................................................... Event ....

struct Event
{
	uint64_t id_;		/* message type */
};

// ................................................................................. LocalEvent ....

struct LocalEvent : Event
{
	uint32_t src_;		/* sender */
	uint32_t dest_;		/* receiver */
};

// ........................................................................ EventHandler<EVENT> ....

template<class EVENT>
class EventHandler
{
public:
	EventHandler()
		: q_("/eventhandler/q_")
	{}

	virtual ~EventHandler() {}

	virtual void Handle(const EVENT & event) = 0;

	Queue<EVENT> q_;
};

// ............................................................................. EventScheduler ....

class EventScheduler
{
public:
	virtual ~EventScheduler() {}

	virtual void Start() = 0;
	virtual void Stop() = 0;
};

// ............................................................................. EventProcessor ....

class EventProcessor
{
public:

	EventProcessor(const std::string & name)
		: name_(name)
	{
	}
 
	virtual ~EventProcessor()
	{
		INVARIANT(schedulers_.empty());
	}

	virtual void Start() = 0;

	virtual void Stop()
	{
		INFO(name_) << "Stopping event schedulers";

		Guard _(&lock_);

		for (auto s : schedulers_) {
			s->Stop();
			delete s;
		}

		schedulers_.clear();
	}

protected:

	void AddEventScheduler(EventScheduler * const scheduler)
	{
		INVARIANT(scheduler);

		Guard _(&lock_);

		schedulers_.push_back(scheduler);
		scheduler->Start();
	}

	const std::string name_;
	PThreadMutex lock_;
	std::list<EventScheduler *> schedulers_;
};

// ......................................................................... EventThread<EVENT> ....

template<class EVENT>
class EventThread : public EventScheduler, public Thread
{
public:

	EventThread(EventHandler<EVENT> * const eventHandler)
		: Thread("/eventthread/")
		, eventHandler_(eventHandler)
	{
		INVARIANT(eventHandler_);
	}

	virtual ~EventThread() {}

	void Start() override
	{
		Thread::StartBlockingThread();
	}

	void Stop() override
	{
		INVARIANT(eventHandler_->q_.IsEmpty());

		Thread::Cancel();
		Thread::Stop();
	}

protected:

	void * ThreadMain() override
	{
		while (true) {
			EVENT event = eventHandler_->q_.Pop();
			eventHandler_->Handle(event);
		}

		DEADEND
		return nullptr;
	}

	EventHandler<EVENT> * const eventHandler_;
};

}
