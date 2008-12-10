#include "SimpleConnection.h"

#include "SimpleBackend.h"

boost::filesystem::path SimpleTCPConnection::getsharepath(std::string sharename)
{
	return backend->getsharepath(sharename);
}

bool SimpleTCPConnection::use_expiremental_resume()
{
	return backend->settings.experimentalresume;
}
