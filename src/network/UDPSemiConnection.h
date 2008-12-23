#ifndef UDP_SEMI_CONNECTION_H
#define UDP_SEMI_CONNECTION_H

class UDPSemiConnection : public SimpleConnection<UDPConn, UDPConnService&> {
	public:
		UDPSemiConnection(boost::asio::io_service& service_, UFTTCore* core_, UDPConnService& cservice)
		: SimpleConnection<UDPConn, UDPConnService&>(service_, core_, cservice)
		{
		}
};
typedef boost::shared_ptr<UDPSemiConnection> UDPSemiConnectionRef;

#endif//UDP_SEMI_CONNECTION_H
