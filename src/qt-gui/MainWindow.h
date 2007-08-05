#ifndef MAIN_H
#define MAIN_H

#include "ui_MainWindow.h"

class MainWindow: public QMainWindow, public Ui::MainWindow
{
	Q_OBJECT

	public:
		MainWindow();
		~MainWindow();

	public slots:
		void RefreshButtonClicked();
		void AddNewServer();

	// thread marshalling stuff
	signals:
		void sigAddNewServer();
	
	public:
		void emitAddNewServer();
};

#endif
