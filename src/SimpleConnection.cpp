#include "SimpleConnection.h"

#include "SimpleBackend.h"

void SimpleTCPConnection::getsharepath(std::string sharename)
{
	backend->getsharepath(this, sharename);
}
