#ifndef _KASYNC_ACTOR_H_
#define _KASYNC_ACTOR_H_

#include <string>
#include <inttypes.h>
#include <list>
#include <boost/lexical_cast.hpp>

#include "core/util.hpp"
#include "core/logger.h"
#include "core/lock.h"
#include "core/inlist.hpp"

#define STR(x) boost::lexical_cast<std::string>(x)

#define ALIGNED(x) __attribute__((aligned(sizeof(x))))

namespace dh_core {

typedef uint64_t MessageType;

class ActorCommand : public InListElement<ActorCommand>
{
public:

    ActorCommand(uint64_t cmd) : cmd_(cmd) {}
    virtual ~ActorCommand() {}

    virtual void Cancel() = 0;

private:

    uint64_t cmd_;
};

class Inbox : public RefCounted
{
public:

    Inbox(const std::string & name)
        : log_(name)
        , lock_(new SpinMutex())
    {}

    // Push a command
    void Push(ActorCommand * cmd);
    // Pop the command to process
    ActorCommand * Pop();

private:

    virtual ~Inbox()
    {
    }

    LogPath log_;
    AutoPtr<Mutex> lock_;
    InList<ActorCommand> cmds_;
};

class Actor
{
public:

    Actor(const std::string & name, Inbox * inbox)
        : log_(name)
        , lock_(new PThreadMutex())
        , inbox_(inbox)
    {}

    virtual ~Actor()
    {
    }

    void Start();

    void Stop()
    {
        stopCondition_.Broadcast();
    }

    void WaitForStop()
    {
        AutoLock _(lock_.Get());
        stopCondition_.Wait((PThreadMutex *) lock_.Get());
    }

    void Schedule(const ActorCommand * cmd);

private:

    virtual bool HandleCommand(const ActorCommand * cmd) = 0;

    void FlushOutbox();
    virtual bool MainLoop();

    struct OutboxMessage : public InListElement<OutboxMessage>
    {
        Ref<Inbox> inbox_;
        ActorCommand * cmd_;
    };

    typedef InList<ActorCommand> InboxQueue;
    typedef InList<OutboxMessage> OutboxQueue;

    LogPath log_;
    AutoPtr<Mutex> lock_;
    Ref<Inbox> inbox_;
    OutboxQueue outbox_;
    WaitCondition stopCondition_;
};

template<class _DATA_, class _CTX_>
class UpdateCommand : public ActorCommand
{

public:

    UpdateCommand(const uint64_t cmd, _DATA_ & data, _CTX_ & ctx = NULL)
        : cmd_(cmd), data_(data), ctx_(ctx)
    {}

    const uint64_t cmd_;
    const _DATA_ data_;
    const _CTX_ ctx_;

} ALIGNED(uint64_t);

template<class _DATA_, class _CTX_>
class RequestCommand : public ActorCommand
{

public:

    RequestCommand(Ref<Inbox> & replyTo, const uint64_t cmd,
                   const _DATA_ & data, const _CTX_ & ctx)
        : replyTo_(replyTo), cmd_(cmd), data_(data), ctx_(ctx)
    {}

    Ref<Inbox> replyTo_;
    const uint64_t cmd_;
    const _DATA_ data_;
    const _CTX_ ctx_;

} ALIGNED(uint64_t);


} // namespace kware {

#endif
