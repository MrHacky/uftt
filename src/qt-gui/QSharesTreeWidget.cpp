#include "QSharesTreeWidget.h"

//#include <QImageDrag>
//#include <QTextDrag>

#include <iostream>

#include "../files/FileInfo.h"

using namespace std;

QSharesTreeWidget::QSharesTreeWidget(QWidget*& widget)
	: QTreeWidget(widget)
{
};

void QSharesTreeWidget::dragEnterEvent(QDragEnterEvent* event)
{
	cout << "event\n";
	if ((event->mimeData()->hasFormat("text/plain"))
	 && (event->mimeData()->text().toStdString().substr(0, 7) == "file://"))
		event->acceptProposedAction();
}

void QSharesTreeWidget::dragMoveEvent(QDragMoveEvent* event)
{
	QWidget::dragMoveEvent(event);
}

void QSharesTreeWidget::dropEvent(QDropEvent* event)
{
	cout << "try\n" << event->mimeData()->text().toStdString() << '\n';
	event->acceptProposedAction();

	FileInfo fi(event->mimeData()->text().toStdString().substr(7));
}
