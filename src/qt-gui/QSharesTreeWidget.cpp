#include "QSharesTreeWidget.h"

//#include <QImageDrag>
//#include <QTextDrag>

#include <iostream>

using namespace std;

QSharesTreeWidget::QSharesTreeWidget(QWidget*& widget)
	: QTreeWidget(widget)
{
	setAcceptDrops(true);
};

void QSharesTreeWidget::dragEnterEvent(QDragEnterEvent* event)
{
	cout << "event\n";
	if (event->mimeData()->hasFormat("text/plain"))
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
}
