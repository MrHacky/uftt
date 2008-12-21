#ifndef ASIO_ONESHOT_TIMER
#define ASIO_ONESHOT_TIMER

#include <boost/asio.hpp>
#include <boost/function.hpp>
#include <boost/shared_ptr.hpp>

struct asio_timer_oneshot_helper {
	boost::function<void()> cb;
	boost::asio::deadline_timer timer;
	void doit() {
		cb();
	}
	asio_timer_oneshot_helper(boost::asio::io_service& service) : timer(service) {};
};

template<typename CB>
inline void asio_timer_oneshot(boost::asio::io_service& service, const boost::posix_time::time_duration& time, const CB& cb)
{
	boost::shared_ptr<asio_timer_oneshot_helper> helper(new asio_timer_oneshot_helper(service));
	helper->cb = cb;
	helper->timer.expires_from_now(time);
	helper->timer.async_wait(boost::bind(&asio_timer_oneshot_helper::doit, helper));
}

template<typename CB>
inline void asio_timer_oneshot(boost::asio::io_service& service, int msecs, const CB& cb)
{
	asio_timer_oneshot(service, boost::posix_time::millisec(msecs), cb);
}

#endif//ASIO_ONESHOT_TIMER
