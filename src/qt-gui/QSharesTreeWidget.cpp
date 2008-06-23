#include "QSharesTreeWidget.h"

#include <QTreeWidgetItem>
#include <QUrl>

#include <boost/foreach.hpp>

#include <iostream>

#include "../files/FileInfo.h"
#include "../SharedData.h"
#include "../Logger.h"
#include "MainWindow.h"

using namespace std;

QSharesTreeWidget::QSharesTreeWidget(QWidget*& widget)
	: QTreeWidget(widget)
{
};

void QSharesTreeWidget::dragEnterEvent(QDragEnterEvent* event)
{
	if (event->mimeData()->hasUrls())
		event->acceptProposedAction();
}

void QSharesTreeWidget::dragMoveEvent(QDragMoveEvent* event)
{
	QWidget::dragMoveEvent(event);
}

void QSharesTreeWidget::dropEvent(QDropEvent* event)
{
	LOG("try=" << event->mimeData()->text().toStdString());
	event->acceptProposedAction();


	BOOST_FOREACH(const QUrl & url, event->mimeData()->urls()) {
		//QString qurl = url.toLocalFile();
		//std::string surl(qurl.toAscii().data());
		//continue;
		string str(url.toLocalFile().toAscii().data());

		if (!str.empty()) {
			// haxxy
			((MainWindow*)this->parent()->parent()->parent())->addLocalShare(str);
			/*
			FileInfoRef fi(new FileInfo(str));
			addFileInfo(*fi);
			
			ShareInfo fs(fi);
			fs.path = str;
			{
				boost::mutex::scoped_lock lock(shares_mutex);
				MyShares.push_back(fs);
			}
			*/
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
