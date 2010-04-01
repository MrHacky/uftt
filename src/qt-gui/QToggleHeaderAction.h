#ifndef Q_TOGGLE_HEADER_ACTION_H
#define Q_TOGGLE_HEADER_ACTION_H

#include <QAction>

class QTreeView;
class QTreeWidget;

class QToggleHeaderAction: public QAction {
	Q_OBJECT

	private:
		QTreeView* view;
		int pos;

	private Q_SLOTS:
		void execute(bool);

	public:
		QToggleHeaderAction(const QString& name_, int pos_, QTreeView* view_);

		static void autoSizeHeaders(QTreeView* view);
		static void addActions(QTreeWidget* widget);
};

#endif//Q_TOGGLE_HEADER_ACTION_H
