#ifndef CORO_H
#define CORO_H

#include <boost/detail/atomic_count.hpp>
#include <boost/intrusive_ptr.hpp>
#include <boost/utility.hpp>

#include "coroutine.hpp"

namespace ext {
	namespace detail { namespace coro {
		typedef coroutine state;

		template <typename T>
		class resumer_intrusive {
			private:
				boost::intrusive_ptr<T> obj;
				state coro;

			public:
				typedef void result_type;

				resumer_intrusive(T* obj_, state coro_)
				: obj(obj_), coro(coro_)
				{}

				#ifdef BOOST_HAS_RVALUE_REFS
					#define A(x) T ## x && t ## x
					#define F(x) std::forward<T ## x>(t ## x)
				#else
					#define A(x) const T ## x & t ## x
					#define F(x) t ## x
				#endif

				void operator()() {
					(*obj)(coro);
				}

				template <typename T1>
				void operator()(A(1)) {
					(*obj)(coro, F(1));
				}

				template <typename T1, typename T2>
				void operator()(A(1), A(2)) {
					(*obj)(coro, F(1), F(2));
				}

				template <typename T1, typename T2, typename T3>
				void operator()(A(1), A(2), A(3)) {
					(*obj)(coro, F(1), F(2), F(3));
				}

				#undef A
				#undef F
		};

		template <typename T>
		class coro: public boost::noncopyable {
			private:
				// we could use a more lightweight atomic here to only do memory fencing
				// on decrementing the counter, which is enough for us, but i haven't
				// found one easily accessible (and as portable as this one) yet
				boost::detail::atomic_count counter;

				T* This()
				{
					return static_cast<T*>(this);
				}

			protected:
				// Can be overidden if custom delete behaviour is needed
				void doDelete()
				{
					delete This();
				}

			public:
				coro() : counter(0) {}

				void incref()
				{
					++counter;
				}

				void decref()
				{
					if (--counter == 0) This()->doDelete();
				}

			public:
				resumer_intrusive<T> starter()
				{
					return resume(state());
				}

				void start()
				{
					// resumer will take initial ownership here
					starter()();
				}

				resumer_intrusive<T> resume(state s)
				{
					return resumer_intrusive<T>(This(), s);
				}

				int stateval(state s)
				{
					state sc = s;
					return coroutine_ref(sc);
				}
		};

		template <typename T>
		void intrusive_ptr_add_ref(coro<T>* ptr)
		{
			ptr->incref();
		}

		template <typename T>
		void intrusive_ptr_release(coro<T>* ptr)
		{
			ptr->decref();
		}
	} }

	class coro: public detail::coro::state {
		public:
			coro(detail::coro::state state)
			: detail::coro::state(state)
			{}

			template <typename T>
			class base: public detail::coro::coro<T> {};

			template <typename T>
			detail::coro::resumer_intrusive<T> operator()(detail::coro::coro<T>* ptr)
			{
				return ptr->resume(*this);
			}
	};
}

#endif//CORO_H
