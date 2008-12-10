#include "SimpleBackendIPv4.h"

#include "NetModuleLinker.h"
#include "SimpleBackend.h"

typedef SimpleBackend<int> SimpleBackendIPv4;

REGISTER_NETMODULE_CLASS(SimpleBackendIPv4);
