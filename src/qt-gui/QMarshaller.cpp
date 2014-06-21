#include "QMarshaller.h"

#include <QMetaType>

QMarshaller::QMarshaller(QObject* parent)
: QObject(parent)
{
	// confirmed: Qt detects and ignores duplicate registrations of the same type(name)
	qRegisterMetaType<QMarshaller::functype>("QMarshaller::functype");
	QObject::connect(
		this, SIGNAL(signal_runfunction (QMarshaller::functype)),
		this, SLOT  (execute_runfunction(QMarshaller::functype))
	);
}
