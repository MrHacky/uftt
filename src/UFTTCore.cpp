#include "UFTTCore.h"

#include "network/NetModuleLinker.h"

UFTTCore::UFTTCore()
{
	netmodules = NetModuleLinker::getNetModuleList(this);
}

