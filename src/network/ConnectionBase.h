#ifndef CONNECTION_BASE_H
#define CONNECTION_BASE_H

#include <string>

#include <boost/asio.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/signals2/signal.hpp>

#include "../UFTTCore.h"

class ConnectionBase {
	private:
	protected:
		boost::asio::io_service& service;
		UFTTCore* core;
	public:
		ConnectionBase(boost::asio::io_service& service_, UFTTCore* core_);
		virtual ~ConnectionBase();

		boost::signals2::signal<void(const TaskInfo&)> sig_progress;
		TaskInfo taskinfo;

		virtual void handle_tcp_connect(std::string name, ext::filesystem::path dlpath, uint32 version) = 0;
};
typedef boost::shared_ptr<ConnectionBase> ConnectionBaseRef;

#endif//CONNECTION_BASE_H
