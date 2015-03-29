#include <iostream>

#include "events.h"
#include "test/unit/unit-test.h"

using namespace std;
using namespace bblocks;

struct TestEvent : LocalEvent
{
	TestEvent(atomic<int> & val)
		: val_(val)
	{}

	atomic<int> & val_;
};

class TestEventHandler : private EventHandler<TestEvent>, private EventProcessor
{
public:

	TestEventHandler()
		: EventProcessor("/test-event-handler/event-processor")
	{}

	void Submit(const TestEvent & event)
	{
		EventHandler<TestEvent>::q_.Push(event);
	}

	void Start() override
	{
		AddEventScheduler(new EventThread<TestEvent>(this));
	}

	void Stop() override
	{
		EventProcessor::Stop();
	}

private:

	void Handle(const TestEvent & event)
	{
		event.val_ += 1;
	}
};

void test_basic()
{
	TestEventHandler t;

	t.Start();

	atomic<int> val(0);

	for (int i = 0; i < 100; i++) {
		t.Submit(TestEvent(val));
	}

	while (val < 100);

	t.Stop();
}

int
main()
{
	LogHelper::InitConsoleLogger();
	RRCpuId::Init();

	test_basic();

	RRCpuId::Destroy();
	LogHelper::DestroyLogger();

	return 0;
}
