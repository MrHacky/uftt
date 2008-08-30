#ifndef BOOST_PP_IS_ITERATING

#ifndef Q_MARSHALLER_H
#define Q_MARSHALLER_H

#include <QObject>
#include <boost/function.hpp>
#include <boost/bind.hpp>

#include <boost/preprocessor/repetition.hpp>
#include <boost/preprocessor/iteration/iterate.hpp>
#include <boost/preprocessor/punctuation/comma_if.hpp>

#ifndef QMASRHALLER_FUNCTION_MAX_ARITY
#	define QMASRHALLER_FUNCTION_MAX_ARITY 10
#endif

class QMarshaller: public QObject
{
	Q_OBJECT
		private:
			typedef boost::function<void(void)> functype;

			template <typename TCallee>
			class MarshallFunctionObject {
				private:
					QMarshaller* marshaller;
					TCallee callee;
				public:
					MarshallFunctionObject(QMarshaller* marshaller_, const TCallee& callee_)
						: marshaller(marshaller_), callee(callee_)
					{
					}

#define BOOST_PP_ITERATION_LIMITS (0, QMASRHALLER_FUNCTION_MAX_ARITY)
#define BOOST_PP_FILENAME_1 "QMarshaller.h" // this file
#include BOOST_PP_ITERATE()
/*
					template <typename T1 , ... , typename Tn>
					void operator()(const T1& t1 , ... , const Tn& tn) {
						marshaller->queue_runfunction(boost::bind(callee, t1, ... , tn));
					}
*/
					typedef void result_type;
			};

			void queue_runfunction(const QMarshaller::functype& func)
			{
				this->signal_runfunction(func);
			}

		Q_SIGNALS:
			void signal_runfunction(const QMarshaller::functype& func);

		protected Q_SLOTS:
			void execute_runfunction(const QMarshaller::functype& func)
			{
				func();
			}

		public:
			QMarshaller(QObject* parent = NULL);

			template <typename TCallee>
			MarshallFunctionObject<TCallee> wrap(const TCallee& callee_)
			{
				return MarshallFunctionObject<TCallee>(this, callee_);
			}
};

#endif//Q_MARSHALLER_H

#else//BOOST_PP_IS_ITERATING

#define n BOOST_PP_ITERATION()

#if n > 0
template <BOOST_PP_ENUM_PARAMS(n, class T)>
#endif
void operator()(BOOST_PP_ENUM_BINARY_PARAMS(n, const T, &t))
{
	marshaller->queue_runfunction(boost::bind(callee
		BOOST_PP_COMMA_IF(n)
		BOOST_PP_ENUM_PARAMS(n,t)
	));
}

#undef n

#endif//BOOST_PP_IS_ITERATING
