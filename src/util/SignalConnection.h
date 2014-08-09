#ifndef SIGNAL_CONNECTION_H
#define SIGNAL_CONNECTION_H

#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/signals2/signal.hpp>

#include "asio_sync_dispatcher.h"

// class which allows binding signal connections to an asio service,
// which is needed for thread-safe connecting/disconnecting
struct SignalConnection {
	public:
		SignalConnection();
		SignalConnection(const SignalConnection& o);
		SignalConnection& operator=(const SignalConnection& o);
		~SignalConnection();

		SignalConnection& operator+=(const SignalConnection& o);

		void disconnect();

		template <typename CALL>
		static SignalConnection connect_wait(boost::asio::io_service& service, const CALL& call)
		{
			return SignalConnection(service, ext::asio::sync_dispatch(service, call));
		}

		template <typename SIG, typename SLOT>
		static SignalConnection connect_wait(boost::asio::io_service& service, boost::signals2::signal<SIG>& signal, const SLOT& slot)
		{
			return connect_wait(service, boost::bind(&boost::signals2::signal<SIG>::connect, &signal, slot, boost::signals2::at_back));
		}

	private:
		// use connect_wait to obtain a usefull SignalConnection
		SignalConnection(boost::asio::io_service& service_, boost::signals2::connection c);
		void do_disconnect();

	private:
		std::vector<boost::signals2::connection> conn;
		boost::asio::io_service* service;
		SignalConnection* next;
};

#endif//SIGNAL_CONNECTION_H
