#include "SimpleConnection.h"

#include "SimpleBackend.h"

boost::filesystem::path SimpleTCPConnection::getsharepath(std::string sharename)
{
	return backend->getsharepath(sharename);
}
