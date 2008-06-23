#ifndef Q_DRAGDROP_TREEWIDGET_H
#define Q_DRAGDROP_TREEWIDGET_H

#include <QTreeWidget>
#include "QDragDropSignalWidget.h"

class QDragDropTreeWidget: public QDragDropSignalWidget<QTreeWidget> {
	public:
		QDragDropTreeWidget(QWidget* parent)
			: QDragDropSignalWidget<QTreeWidget>(parent) {
		}
};

#endif//Q_DRAGDROP_TREEWIDGET_H
