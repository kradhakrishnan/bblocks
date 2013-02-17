#include "core/actor.h"

using namespace dh_core;

//
// Inbox
//
void
Inbox::Push(ActorCommand * cmd)
{
    AutoLock _(lock_.Get());

    // scan to see if need to schedule the inbox
    const bool schd = cmds_.IsEmpty();
    // push the command into the inbox
    cmds_.Push(cmd);

    if (schd) {
        Scheduler::Instance().Schedule(this);
    }
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
    INVARIANT(actor_ == actor);

    actor_ = NULL;
}

bool
Inbox::Execute()
{
    AutoLock _(lock_.Get());

    ActorCommand * cmd = cmds_.Pop();
    if (actor_) {
        bool ok = actor_->Handle(cmd);
        if (!ok) {
            // requeue this message
            cmds_.Push(cmd);
            return false;
        }

        return !cmds_.IsEmpty();
    }

    ASSERT(!actor_);
    delete cmd;

    return !cmds_.IsEmpty();

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
