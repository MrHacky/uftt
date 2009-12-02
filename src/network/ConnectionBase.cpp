#include "ConnectionBase.h"

#include "../UFTTCore.h"

ConnectionBase::ConnectionBase(boost::asio::io_service& service_, UFTTCoreRef core_)
: service(service_)
, core(core_)
{
}

ConnectionBase::~ConnectionBase()
{
}
