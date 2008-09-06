#include "SimpleConnection.h"

#include "SimpleBackend.h"

void SimpleTCPConnection::getsharepath(std::string sharename)
{
	backend->getsharepath(this, sharename);
}

void SimpleTCPConnection::sig_download_ready(std::string url)
{
	backend->sig_download_ready(url);
}
