#include "QToggleHeaderAction.moc"

#include <QHeaderView>
#include <QTreeView>
#include <QTreeWidget>

QToggleHeaderAction::QToggleHeaderAction(QTreeView* view_, const QString& name_, int pos_)
: view(view_)
, pos(pos_)
, QAction(name_, view_)
{
	QObject::connect(this, SIGNAL(triggered(bool)), this, SLOT(execute(bool)));
	this->setCheckable(true);
	this->setChecked(!view->isColumnHidden(pos));
}

void QToggleHeaderAction::execute(bool checked)
{
	if (checked)
		view->showColumn(pos);
	else
		view->hideColumn(pos);

	autoSizeHeaders(view);
}

void QToggleHeaderAction::autoSizeHeaders(QTreeView* view)
{
	// do some advanced collumn resizing
	int numcols = view->header()->count();

	// get the total view width
	view->header()->setStretchLastSection(true);
	int origtotal = 0;
	for (int i = 0; i < numcols; ++i) {
		view->resizeColumnToContents(i);
		origtotal += view->columnWidth(i);
	}

	// get the amount of width needed, and the amount of shown collumns
	view->header()->setStretchLastSection(false);
	std::vector<int> newwidth(numcols);
	int newtotal = 0;
	int rnumcols = 0;
	for (int i = 0; i < numcols; ++i) {
		view->resizeColumnToContents(i);
		newwidth[i] = view->columnWidth(i);
		newtotal += newwidth[i];
		if (newwidth[i] > 0) ++rnumcols;
	}

	// set the new widths
	view->header()->setStretchLastSection(true);
	if (newtotal < origtotal) {
		int add = (origtotal - newtotal - 1) / rnumcols;
		for (int i = 0; i < numcols; ++i) {
			if (newwidth[i] > 0) {
				newwidth[i] += add;
				view->setColumnWidth(i, newwidth[i]);
			}
		}
	}
}

void QToggleHeaderAction::addActions(QTreeWidget* widget)
{
	widget->header()->setContextMenuPolicy(Qt::ActionsContextMenu);
	for(int i = 0; i < widget->header()->count(); ++i)
		widget->header()->addAction(new QToggleHeaderAction(widget, widget->headerItem()->text(i), i));
}
