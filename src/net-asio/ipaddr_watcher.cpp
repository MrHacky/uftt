#include "ipaddr_watcher.h"

#include <set>

#include <boost/foreach.hpp>
#include <boost/thread.hpp>

typedef std::pair<boost::asio::ip::address,boost::asio::ip::address> addrwithbroadcast;

#define IPADDR_WATCHER_INCLUDE
#include "ipaddr_watcher_osimpl.cpp"

// above include should implement ipv4_osimpl and ipv6_osimpl, which should implement this interface:
struct fake_osimpl_interface {
	// These three functions are always called from a single thread
	void init(); // initialize sync waiter
	bool sync_wait(); // wait for change in interfaces, return false on error
	void close(); // clean up sync waiter

	// these can be called from a different thread
	std::set<struct Addr> getlist(); // return list of interfaces (Addr(v4/v6) = addrwithbroadcast/boost::asio::ip::address)
	bool cancel_wait(); // attempt to cancel sync_wait call, return true only when 100% sure sync_wait will return soon
};

template <typename Watcher, typename Addr, typename OSImpl>
struct ipaddr_watcher_implementation {
	boost::asio::io_service& service;
	boost::thread thread;
	boost::asio::deadline_timer timer;
	boost::mutex watcher_mutex;
	std::set<Addr> addrlist;
	Watcher* watcher;
	OSImpl osi;

	ipaddr_watcher_implementation(boost::asio::io_service& service_)
	: service(service_), timer(service), watcher(NULL)
	{
	}

	void update_list()
	{
		// is only called from callbacks through service (timer.async_wait and service.post)
		// to service must be valid here, and watcher doesn't need a lock
		if (!watcher) return;
		std::set<Addr> naddrlist = osi.getlist();
		bool change = false;
		BOOST_FOREACH(const Addr& naddr, naddrlist)
			if (!addrlist.count(naddr))
				change = true, watcher->add_addr(naddr);
		BOOST_FOREACH(const Addr& oaddr, addrlist)
			if (!naddrlist.count(oaddr))
				change = true, watcher->del_addr(oaddr);
		if (change) {
			addrlist = naddrlist;
			watcher->new_list(addrlist);
		}
	}

	void notify_list(boost::shared_ptr<ipaddr_watcher_implementation> impl)
	{
		update_list();
		timer.expires_from_now(boost::posix_time::millisec(500));
		timer.async_wait(boost::bind(&ipaddr_watcher_implementation::update_list, impl));
	}

	void thread_loop(boost::shared_ptr<ipaddr_watcher_implementation> impl)
	{
		osi.init();
		int wait = 0;
		for (;;) {
			{
				boost::mutex::scoped_lock lock(watcher_mutex);
				if (watcher) // (watcher != NULL) => (service is valid)
					service.post(boost::bind(&ipaddr_watcher_implementation::notify_list, impl, impl));
				else
					break;
			}
			if (osi.sync_wait()) {
				if (wait > 0) wait -= 600;
			} else {
				if (wait < 60000) wait += 600;
				boost::this_thread::sleep(boost::posix_time::milliseconds(wait));
			}
		};
		osi.close();
	}

	void async_wait(Watcher* watcher_, boost::shared_ptr<ipaddr_watcher_implementation> impl)
	{
		{
			boost::mutex::scoped_lock lock(watcher_mutex);
			watcher = watcher_;
		}
		thread = boost::thread(boost::bind(&ipaddr_watcher_implementation::thread_loop, this, impl));
	}

	void close()
	{
		{
			boost::mutex::scoped_lock lock(watcher_mutex);
			watcher = NULL;
		}
		if (osi.cancel_wait())
			thread.join();
		else
			thread.detach();
	}
};

class ipv4_watcher::implementation : public ipaddr_watcher_implementation<ipv4_watcher, addrwithbroadcast, ipv4_osimpl> {
	public:
		implementation(boost::asio::io_service& service)
		: ipaddr_watcher_implementation<ipv4_watcher, addrwithbroadcast, ipv4_osimpl>(service)
		{}
};

class ipv6_watcher::implementation : public ipaddr_watcher_implementation<ipv6_watcher, boost::asio::ip::address, ipv6_osimpl> {
	public:
		implementation(boost::asio::io_service& service)
		: ipaddr_watcher_implementation<ipv6_watcher, boost::asio::ip::address, ipv6_osimpl>(service)
		{}
};

ipv4_watcher::ipv4_watcher(boost::asio::io_service& service)
: impl(new implementation(service))
{
}

ipv4_watcher::~ipv4_watcher()
{
	impl->close();
}

void ipv4_watcher::async_wait()
{
	impl->async_wait(this, impl);
}

ipv6_watcher::ipv6_watcher(boost::asio::io_service& service)
: impl(new implementation(service))
{
}

ipv6_watcher::~ipv6_watcher()
{
	impl->close();
}

void ipv6_watcher::async_wait()
{
	impl->async_wait(this, impl);
}
