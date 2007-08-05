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

	public:
		MainWindow();
		~MainWindow();

	public slots:
		void RefreshButtonClicked();
		void AddNewServer();
		void AddNewShare(std::string str, SHA1 hash);

	// thread marshalling stuff
	signals:
		void sigAddNewServer();
		void sigAddNewShare(std::string, SHA1);
	
	public:
		void emitAddNewServer();
		void emitAddNewShare(std::string, SHA1);
		void emitAddNewShare2(std::string a1, SHA1 a2)
		{
			sigAddNewShare(a1, a2);
		}
};

#endif
