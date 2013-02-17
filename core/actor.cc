#include "core/actor.h"

using namespace dh_core;

//
// Inbox
//
void
Inbox::Push(ActorCommand * cmd)
{
    AutoLock _(lock_.Get());
    cmds_.Push(cmd);
}

ActorCommand *
Inbox::Pop()
{
    AutoLock _(lock_.Get());
    return cmds_.Pop();
}

void
Inbox::AttachActor(Actor * actor)
{
    AutoLock _(lock_.Get());

    INVARIANT(!actor_);
    actor_ = actor;
}

void
Inbox::DetachActor(Actor * actor)
{
    AutoLock _(lock_.Get());

    INVARIANT(actor_);
    actor_ = NULL;
}

//
// Actor
//
void
Actor::Start()
{
    INFO(log_) << "Starting actor " << name_;

    // send yourself a message to start up
    SendImm(inbox_.Get(), MakeInfoCommand(Actor::STARTUP));
}

void
Actor::Stop()
{
    stopCondition_.Broadcast();
}

void
Actor::WaitForStop()
{
    AutoLock _(lock_.Get());
    stopCondition_.Wait((PThreadMutex *) lock_.Get());
}

void
Actor::Send(Inbox * inbox, ActorCommand * cmd)
{
    ASSERT(lock_->IsOwner());
    outbox_.push_back(OutboxMessage(inbox, cmd));
}

void
Actor::SendImm(Inbox * inbox, ActorCommand * cmd)
{
    inbox->Push(cmd);
}

void
Actor::FlushOutbox()
{
    ASSERT(lock_->IsOwner());

    for (OutboxQueue::iterator it = outbox_.begin(); it != outbox_.end();
         ++it) {
        OutboxMessage msg = *it;
        msg.inbox_->Push(msg.cmd_);
    }

    outbox_.clear();

}
