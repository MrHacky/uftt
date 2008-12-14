#ifndef IPADDR_WATCHER_H
#define IPADDR_WATCHER_H

#include <boost/signals.hpp>
#include <boost/asio.hpp>
#include <boost/scoped_ptr.hpp>
#include <boost/utility.hpp>

#include <set>

class ipv4_watcher : private boost::noncopyable {
	private:
		class implementation;
		boost::scoped_ptr<implementation> impl;
	public:
		ipv4_watcher(boost::asio::io_service& service);
		~ipv4_watcher();

		//boost::signal<void(boost::asio::ip::address)> add_addr_nbc;
		//boost::signal<void(boost::asio::ip::address)> del_addr;
		//boost::signal<void(std::set<boost::asio::ip::address>)> new_list;

		boost::signal<void(std::pair<boost::asio::ip::address,boost::asio::ip::address>)> add_addr;
		boost::signal<void(std::pair<boost::asio::ip::address,boost::asio::ip::address>)> del_addr;
		boost::signal<void(std::set<std::pair<boost::asio::ip::address,boost::asio::ip::address> >)> new_list;
		void async_wait();
};

class ipv6_watcher : private boost::noncopyable {
	private:
		class implementation;
		boost::scoped_ptr<implementation> impl;
	public:
		ipv6_watcher(boost::asio::io_service& service);
		~ipv6_watcher();

		boost::signal<void(boost::asio::ip::address)> add_addr;
		boost::signal<void(boost::asio::ip::address)> del_addr;
		boost::signal<void(std::set<boost::asio::ip::address>)> new_list;

		void async_wait();
};

#endif//IPADDR_WATCHER_H
