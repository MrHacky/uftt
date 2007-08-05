#include "QSharesTreeWidget.h"

#include <QTreeWidgetItem>

#include <boost/foreach.hpp>
#include <boost/algorithm/string.hpp>

#include <iostream>

#include "../files/FileInfo.h"
#include "../SharedData.h"

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
	
	string text = event->mimeData()->text().toStdString();
	vector<string> paths;
	
	boost::split(paths, text, boost::is_any_of("\n"));

	BOOST_FOREACH(const string& str, paths)
	if (!str.empty()) {
		FileInfoRef fi( new FileInfo(str.substr(7)));
		addFileInfo(*fi);
		
		ShareInfo fs(fi);
		fs.path = str.substr(7);
		{
			boost::mutex::scoped_lock lock(shares_mutex);
			MyShares.push_back(fs);
		}
	}
}

void QSharesTreeWidget::addFileInfo(const FileInfo& fi, QTreeWidgetItem* parent)
{
	QTreeWidgetItem* rwi;
	if (parent)
	 rwi = new QTreeWidgetItem(parent, 0);
	else
	 rwi = new QTreeWidgetItem(this, 0);

	rwi->setText(0, fi.name.c_str());

	BOOST_FOREACH(const FileInfoRef& iter, fi.files) {
		addFileInfo(*iter, rwi);
	}
	
}
