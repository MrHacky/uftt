#include "SignalConnection.h"

SignalConnection::SignalConnection()
: service(NULL), next(NULL)
{
};

SignalConnection::SignalConnection(boost::asio::io_service& service_, boost::signals::connection c)
: service(&service_), next(NULL)
{
	if (c.connected())
		conn.push_back(c);
}

SignalConnection::SignalConnection(const SignalConnection& o)
: next(NULL)
{
	*this = o;
}

SignalConnection& SignalConnection::operator=(const SignalConnection& o)
{
	service = o.service;
	conn = o.conn;
	if (next) delete next;
	if (o.next)
		next = new SignalConnection(*o.next);
	else
		next = NULL;
	return *this;
}

SignalConnection::~SignalConnection()
{
	if (next) delete next;
}

SignalConnection& SignalConnection::operator+=(const SignalConnection& o)
{
	if (conn.empty() && !next)
		return *this = o;
	if (o.conn.empty() && !o.next)
		return *this;
	if (o.service != service) {
		if (next)
			*next += o;
		else
			next = new SignalConnection(o);
	} else {
		conn.insert(conn.end(), o.conn.begin(), o.conn.end());
		if (o.next)
			*this += *o.next;
	}
	return *this;
}

void SignalConnection::disconnect()
{
	if (next) next->disconnect();
	if (conn.empty()) return;
	if (!service)
		do_disconnect();
	else
		ext::asio::sync_dispatch(*service, boost::bind(&SignalConnection::do_disconnect, this));
};

void SignalConnection::do_disconnect()
{
	for (size_t i = 0; i < conn.size(); ++i)
		conn[i].disconnect();
	conn.clear();
}
