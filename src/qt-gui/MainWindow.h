#ifndef MAIN_H
#define MAIN_H

#include "ui_MainWindow.h"

#include <map>

class QTreeWidgetItem;

class MainWindow: public QMainWindow, public Ui::MainWindow
{
	Q_OBJECT

	private:
		std::map<SHA1, QTreeWidgetItem*> treedata;

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
		void AddNewFileInfo(void* data);

	// thread marshalling stuff
	signals:
		void sigAddNewServer();
		void sigAddNewShare(std::string, SHA1);
		void sigAddNewFileInfo(void*);
		
	
	public:
		void emitAddNewServer();
		void emitAddNewShare(std::string, SHA1);
		void emitAddNewFileInfo(void*);
};

#endif
