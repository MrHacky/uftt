#include "QMarshaller.moc"

#include <QMetaType>

QMarshaller::QMarshaller(QObject* parent)
: QObject(parent)
{
	qRegisterMetaType<QMarshaller::vvfunc>("QMarshaller::vvfunc");
	QObject::connect(
		this, SIGNAL(queue_runfunction(QMarshaller::vvfunc)),
		this, SLOT(execute_runfunction(QMarshaller::vvfunc))
	);
}
