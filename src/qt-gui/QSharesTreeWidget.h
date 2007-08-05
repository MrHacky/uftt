#ifndef QSHARETREEWIDGET_H
#define QSHARETREEWIDGET_H

#include <QTreeWidget>
#include <QDragEnterEvent>

#include "../files/FileInfo.h"

class QSharesTreeWidget: public QTreeWidget {
	public:
		QSharesTreeWidget(QWidget*& widget);

		void dragMoveEvent(QDragMoveEvent* event);
		void dragEnterEvent(QDragEnterEvent *event);
		void dropEvent(QDropEvent* event);

		void addFileInfo(const FileInfo& fi, QTreeWidgetItem* parent = NULL);
};

#endif
