#ifndef Q_MARSHALLER_H
#define Q_MARSHALLER_H

#include <QObject>
#include <boost/function.hpp>
#include <boost/bind.hpp>

class QMarshaller: public QObject
{
	Q_OBJECT
		private:
			typedef boost::function<void(void)> vvfunc;

			template <typename TCallee>
			class MarshallFunctionObject {
				private:
					TCallee callee;
					QMarshaller* marshaller;
				public:
					MarshallFunctionObject(QMarshaller* marshaller_, const TCallee& callee_)
						:  marshaller(marshaller_), callee(callee_)
					{
					}

					template <typename T1>
					void operator()(const T1& t1) {
						marshaller->queue_runfunction(boost::bind(callee, t1));
					}

					typedef void result_type;
			};

		Q_SIGNALS:
			void queue_runfunction(QMarshaller::vvfunc func);

		protected Q_SLOTS:
			void execute_runfunction(QMarshaller::vvfunc func)
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
