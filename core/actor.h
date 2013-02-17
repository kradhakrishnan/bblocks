#ifndef _KASYNC_ACTOR_H_
#define _KASYNC_ACTOR_H_

#include <string>
#include <inttypes.h>
#include <list>

#include "core/util.hpp"
#include "core/logger.h"
#include "core/lock.h"
#include "core/inlist.hpp"
#include "core/scheduler.h"

namespace dh_core {

typedef uint64_t MessageType;

class Actor;

/**
 *
 */
class ActorCommand : public InListElement<ActorCommand>
{
public:

    ActorCommand(uint64_t cmd) : cmd_(cmd) {}
    virtual ~ActorCommand() {}

    // virtual void Cancel() = 0;

private:

    uint64_t cmd_;
};

/**
 *
 */
class Inbox : public RefCounted, public Schedulable
{
public:

    Inbox(const std::string & name)
        : Schedulable(name, RRCpuId::Instance().GetId())
        , log_("/inbox/" + name)
        , lock_(new SpinMutex())
        , actor_(NULL)
    {}

    // Push a command
    void Push(ActorCommand * cmd);
    // Attach an actor for the messages
    void AttachActor(Actor * actor);
    // Detach existing actor
    void DetachActor(Actor * actor);

    // functions from Schedulable
    virtual bool Execute();

private:

    virtual ~Inbox()
    {
    }

    LogPath log_;
    AutoPtr<Mutex> lock_;
    Actor * actor_;
    InList<ActorCommand> cmds_;
};

/**
 *
 */
class Actor
{
public:

    friend class Inbox;

    // Action commands
    static const uint64_t STARTUP = 0; // STARTUP
    static const uint64_t CUSTOM_COMMAND = 64; // Child specific commands

    Actor(const std::string & name)
        : name_(name)
        , log_("/actor/" + name_)
        , lock_(new PThreadMutex())
        , inbox_(new Inbox(name_))
    {
        inbox_->AttachActor(this);
    }

    virtual ~Actor()
    {
        INFO(log_) << "Actor " << name_ << " destroyed";
        Stop();
    }

    void Start();
    void WaitForStop();


private:

    // handle the command
    // to be implemented by every actor
    virtual bool Handle(const ActorCommand * cmd) = 0;
 
    // Send a message (will be flushed while swapping context)
    void Send(Inbox * inbox, ActorCommand * cmd);
    // Send a message immediately
    void SendImm(Inbox * inbox, ActorCommand * cmd);
    // logically stop the actor
    void Stop();
   // flush out the messages in the outbox
    void FlushOutbox();

    /**
     */
    struct OutboxMessage : public InListElement<OutboxMessage>
    {
        OutboxMessage(Inbox * inbox, ActorCommand * cmd)
            : inbox_(inbox), cmd_(cmd)
        {}

        Ref<Inbox> inbox_;
        ActorCommand * cmd_;
    };

    typedef InList<ActorCommand> InboxQueue;
    typedef std::list<OutboxMessage> OutboxQueue;

    const std::string name_;
    LogPath log_;
    AutoPtr<Mutex> lock_;
    Ref<Inbox> inbox_;
    OutboxQueue outbox_;
    WaitCondition stopCondition_;
};

/**
 *
 */
template<class _DATA_, class _CTX_>
class InfoCommand : public ActorCommand
{

public:

    InfoCommand(const uint64_t cmd, const _DATA_ & data, const _CTX_ & ctx)
        : ActorCommand(cmd)
        , data_(data)
        , ctx_(ctx)
    {}

    const _DATA_ data_;
    const _CTX_ ctx_;

} ALIGNED(uint64_t);

template<class _DATA_, class _CTX_>
InfoCommand<_DATA_, _CTX_> * MakeInfoCommand(const uint64_t cmd,
                                            _DATA_ & data,
                                            _CTX_ & ctx)
{
    return new InfoCommand<_DATA_, _CTX_>(cmd, data, ctx);
}

InfoCommand<int, int> * MakeInfoCommand(const uint64_t cmd)
{
    return new InfoCommand<int, int>(cmd, 0, 0);
}

/**
 *
 */
template<class _DATA_, class _CTX_>
class RequestCommand : public ActorCommand
{

public:

    RequestCommand(Ref<Inbox> & replyTo, const uint64_t cmd,
                   const _DATA_ & data, const _CTX_ & ctx)
        : replyTo_(replyTo)
        , cmd_(cmd)
        , data_(data)
        , ctx_(ctx)
    {}

    Ref<Inbox> replyTo_;
    const uint64_t cmd_;
    const _DATA_ data_;
    const _CTX_ ctx_;

} ALIGNED(uint64_t);


} // namespace kware {

#endif
