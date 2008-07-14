#ifndef Q_DRAGDROP_SIGNAL_WIDGET_H
#define Q_DRAGDROP_SIGNAL_WIDGET_H

#include <QWidget>

class QDragDropSignalEmitter: public QObject {
	Q_OBJECT

	public:
		QDragDropSignalEmitter(QObject* parent = 0)
			: QObject(parent)
		{
		}

		void dragMoveEmit(QDragMoveEvent* evt)
		{
			this->dragMoveTriggered(evt);
		}

		void dragEnterEmit(QDragEnterEvent* evt)
		{
			this->dragEnterTriggered(evt);
		}

		void dropEmit(QDropEvent* evt)
		{
			this->dropTriggered(evt);
		}

	Q_SIGNALS:
		void dragMoveTriggered(QDragMoveEvent*);
		void dragEnterTriggered(QDragEnterEvent*);
		void dropTriggered(QDropEvent*);


};

//template<class BaseWidget = QWidget>
template<class BaseWidget>
class QDragDropSignalWidget: public BaseWidget {
	private:
		QDragDropSignalEmitter* emitter;

	private:
		virtual void dragMoveEvent(QDragMoveEvent* evt)
		{
			this->emitter->dragMoveEmit(evt);
		}

		virtual void dragEnterEvent(QDragEnterEvent *evt)
		{
			this->emitter->dragEnterEmit(evt);
		}

		virtual void dropEvent(QDropEvent* evt)
		{
			this->emitter->dropEmit(evt);
		}

	public:
		QDragDropSignalWidget(QWidget * parent = 0)
			: BaseWidget(parent)
		{
			this->emitter = new QDragDropSignalEmitter(this);
			this->setAcceptDrops(true);
		}

		QDragDropSignalEmitter* getDragDropEmitter()
		{
			return this->emitter;
		}
};

#endif//Q_DRAGDROP_SIGNAL_WIDGET_H
