#ifndef SIMPLE_TCP_CONNECTION_H
#define SIMPLE_TCP_CONNECTION_H

#include "SimpleConnection.h"

class SimpleTCPConnection : public SimpleConnection<boost::asio::ip::tcp::socket, boost::asio::io_service&> {
	public:
		SimpleTCPConnection(boost::asio::io_service& service_, UFTTCoreRef core_)
		: SimpleConnection<boost::asio::ip::tcp::socket, boost::asio::io_service&>(service_, core_, service_)
		{
		}
};
typedef boost::shared_ptr<SimpleTCPConnection> SimpleTCPConnectionRef;

#endif//SIMPLE_CONNECTION_H

