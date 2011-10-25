#ifndef ASIO_SYNC_DISPATCHER_H
#define ASIO_SYNC_DISPATCHER_H

#include <boost/thread.hpp>
#include <boost/utility/result_of.hpp>

namespace ext { namespace asio {

	// implements sync_dispatch function, which if called with a service and a callable
	// function object, runs the function object in that service, waits for it to finish,
	// and returns the value it returned

	// helper struct
	template <typename T, typename R>
	struct sync_dispatcher {
		typedef void result_type;

		const T* call;
		R result;

		boost::mutex mutex;
		boost::condition_variable condvar;
		bool done;

		sync_dispatcher(const T* call_)
			: call(call_), done(false) {};

		void operator()() {
			result = (*call)();
			boost::mutex::scoped_lock lock(mutex);
			done = true;
			condvar.notify_all();
		};

		R getResult() { return result; }
	};

	// partial specialization for void
	template <typename T>
	struct sync_dispatcher<T, void> {
		typedef void result_type;

		const T* call;

		boost::mutex mutex;
		boost::condition_variable condvar;
		bool done;

		sync_dispatcher(const T* call_)
			: call(call_), done(false) {};

		void operator()() {
			(*call)();
			boost::mutex::scoped_lock lock(mutex);
			done = true;
			condvar.notify_all();
		};

		void getResult() {};
	};

	template <typename T>
	typename boost::result_of<T()>::type sync_dispatch(boost::asio::io_service& service, const T& cb)
	{
		sync_dispatcher<T, typename boost::result_of<T()>::type> dispatcher(&cb);

		service.dispatch(boost::bind(boost::ref(dispatcher)));

		{
			boost::mutex::scoped_lock lock(dispatcher.mutex);
			while (!dispatcher.done) dispatcher.condvar.wait(lock);
		}

		return dispatcher.getResult(); // function call to allow for void
	}

} } // ext::asio

#endif
