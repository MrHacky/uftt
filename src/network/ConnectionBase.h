#ifndef CONNECTION_BASE_H
#define CONNECTION_BASE_H

#include <string>

#include <boost/asio.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/signal.hpp>

#include "../UFTTCore.h"

class ConnectionBase {
	private:
	protected:
		boost::asio::io_service& service;
		UFTTCoreRef core;
	public:
		ConnectionBase(boost::asio::io_service& service_, UFTTCoreRef core_);
		virtual ~ConnectionBase();

		boost::signal<void(const TaskInfo&)> sig_progress;
		TaskInfo taskinfo;

		virtual void handle_tcp_connect(std::string name, boost::filesystem::path dlpath, uint32 version) = 0;
};
typedef boost::shared_ptr<ConnectionBase> ConnectionBaseRef;

#endif//CONNECTION_BASE_H
