#ifndef MAIN_H
#define MAIN_H

#include "QTMain.h"
#include "ui_MainWindow.h"

#include <map>

#include "../sha1/SHA1.h"
#include "../JobRequest.h"
#include "../files/FileInfo.h"

class QTreeWidgetItem;
class QCloseEvent;

class MainWindow: public QMainWindow, public Ui::MainWindow
{
	Q_OBJECT

	private:
		QTMain& mainimpl;
		std::map<SHA1, QTreeWidgetItem*> treedata;
		std::map<SHA1, FileInfoRef> dirdata;

		void StartDownload(FileInfoRef fi, const fs::path& path);

	protected:
		virtual void closeEvent(QCloseEvent * event);

	private Q_SLOTS:
		void DragStart(QTreeWidgetItem*, int);
		void StartDownload();
		void onDragEnterTriggered(QDragEnterEvent* evt);
		void onDragMoveTriggered(QDragMoveEvent* evt);
		void onDropTriggered(QDropEvent* evt);
	public:
		MainWindow(QTMain& mainimpl_);
		~MainWindow();

	public Q_SLOTS:
		void RefreshButtonClicked();
		void AddNewServer();
		void AddNewShare(std::string str, SHA1 hash);
		void NewTreeInfo(JobRequestRef);
		void addSimpleShare(std::string sharename);
		void addLocalShare(std::string url);
};

class LogHelper: public QObject {
	Q_OBJECT

	Q_SIGNALS:
		void logAppend(QString);

	public:
		void append(QString);
};

#endif
