#include "MainWindow.moc"

#include <iostream>

#include "../SharedData.h"
#include "../network/Packet.h"

using namespace std;

MainWindow::MainWindow()
{
	setupUi(this);

	connect(RefreshButton, SIGNAL(clicked()), this, SLOT(RefreshButtonClicked()));
	
	connect(this, SIGNAL(sigAddNewServer()), this, SLOT(AddNewServer()));
}

MainWindow::~MainWindow()
{
}

void MainWindow::RefreshButtonClicked()
{
	JobRequest job;
	job.type = PT_QUERY_SERVERS; // refresh
	{
		boost::mutex::scoped_lock lock(jobs_mutex);
		JobQueue.push_back(job);
	}
}

void MainWindow::AddNewServer()
{
	cout << "AddNewServer()" << endl;
}

void MainWindow::emitAddNewServer()
{
	cout << ">emitAddNewServer()" << endl;
	emit sigAddNewServer();
	cout << "<emitAddNewServer()" << endl;
}
