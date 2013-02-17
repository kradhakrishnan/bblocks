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
