#ifndef MAIN_H
#define MAIN_H

#include "ui_MainWindow.h"

#include <map>

#include "../sha1/SHA1.h"
#include "../JobRequest.h"

class QTreeWidgetItem;

class MainWindow: public QMainWindow, public Ui::MainWindow
{
	Q_OBJECT

	private:
		std::map<SHA1, QTreeWidgetItem*> treedata;
		std::map<SHA1, FileInfoRef> dirdata;

		void StartDownload(FileInfoRef fi, const fs::path& path);

	private slots:
		void DragStart(QTreeWidgetItem*, int);
		void StartDownload();

	public:
		MainWindow();
		~MainWindow();

	public slots:
		void RefreshButtonClicked();
		void AddNewServer();
		void AddNewShare(std::string str, SHA1 hash);
		void NewTreeInfo(JobRequestRef);

	// thread marshalling stuff
	signals:
		void sigAddNewServer();
		void sigAddNewShare(std::string, SHA1);
		void sigNewTreeInfo(JobRequestRef);

	public:
		void emitAddNewServer();
		void emitAddNewShare(std::string, SHA1);
		void emitNewTreeInfo(JobRequestRef);
};

#endif
