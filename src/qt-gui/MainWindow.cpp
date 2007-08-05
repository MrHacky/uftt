#include "MainWindow.moc"

#include <iostream>

#include "../SharedData.h"
#include "../network/Packet.h"

using namespace std;

MainWindow::MainWindow()
{
	qRegisterMetaType<std::string>("std::string");
	qRegisterMetaType<SHA1>("SHA1");

	setupUi(this);

	connect(RefreshButton, SIGNAL(clicked()), this, SLOT(RefreshButtonClicked()));
	
	connect(this, SIGNAL(sigAddNewServer()), this, SLOT(AddNewServer()));
	connect(this, SIGNAL(sigAddNewShare(std::string, SHA1)), this, SLOT(AddNewShare(std::string, SHA1)));
	//connect(this, SIGNAL(sigAddNewShare()), this, SLOT(AddNewShare()));
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
	emit sigAddNewServer();
}

void MainWindow::AddNewShare(std::string str, SHA1 hash)
{
	cout << "newshare:" << str << endl;
}

void MainWindow::emitAddNewShare(std::string str, SHA1 hash)
{
	cout << ">emit!" << str << endl;
	emit sigAddNewShare(str, hash);
	cout << "<emit!" << str << endl;
}
