#include "bblocks.h"
#include "schd/thread-pool.h"

using namespace bblocks;

void
BBlocks::Start()
{
	Start(SysConf::NumCores());
}

void
BBlocks::Start(const uint32_t ncores)
{
	/*
	 * We need to init the current thread context to enable buffering to schedule
	 */
	ThreadCtx::Init(/*tinst=*/ NULL);

	NonBlockingThreadPool::Instance().Start(ncores);
}

void
BBlocks::Shutdown()
{
	NonBlockingThreadPool::Instance().Shutdown();
	ThreadCtx::Cleanup();
}

void
BBlocks::Wait()
{
	NonBlockingThreadPool::Instance().Wait();
}

void
BBlocks::Wakeup()
{
	NonBlockingThreadPool::Instance().Wakeup();
}

void
BBlocks::Schedule(ThreadRoutine * r)
{
	NonBlockingThreadPool::Instance().Schedule(r);
}

void
BBlocks::ScheduleBarrier(ThreadRoutine * r)
{
	NonBlockingThreadPool::Instance().ScheduleBarrier(r);
}

bool
BBlocks::ShouldYield()
{
	return NonBlockingThreadPool::Instance().ShouldYield();
}

const size_t
BBlocks::ncpu()
{
	return NonBlockingThreadPool::Instance().ncpu();
}
