#ifndef CONNECTION_BASE_H
#define CONNECTION_BASE_H

#include <boost/asio.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/signal.hpp>

#include "../UFTTCore.h"

class ConnectionBase {
	private:
	protected:
		boost::asio::io_service& service;
		UFTTCore* core;
	public:
		ConnectionBase(boost::asio::io_service& service_, UFTTCore* core_);
		virtual ~ConnectionBase();

		boost::signal<void(const TaskInfo&)> sig_progress;
		TaskInfo taskinfo;
};
typedef boost::shared_ptr<ConnectionBase> ConnectionBaseRef;

#endif//CONNECTION_BASE_H
