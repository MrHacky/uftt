#include "MainWindow.moc"

#include <iostream>

#include <QTreeWidgetItem>

#include <boost/foreach.hpp>

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
	connect(this, SIGNAL(sigAddNewFileInfo(void*)), this, SLOT(AddNewFileInfo(void*)));
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
	cout << "TODO?: AddNewServer()" << endl;
}

void MainWindow::emitAddNewServer()
{
	emit sigAddNewServer();
}

void MainWindow::emitAddNewFileInfo(void* data)
{
	emit sigAddNewFileInfo(data);
}

void MainWindow::AddNewShare(std::string str, SHA1 hash)
{
	QTreeWidgetItem* rwi = treedata[hash];
	if (rwi == NULL) {
		rwi = new QTreeWidgetItem(OthersSharesTree, 0);
		rwi->setText(0, str.c_str());
		treedata[hash] = rwi;
	}
	if (rwi->childCount() == 0) {
		JobRequest job;
		job.type = PT_QUERY_CHUNK; // refresh
		job.hash = hash;
		{
			boost::mutex::scoped_lock lock(jobs_mutex);
			JobQueue.push_back(job);
		}
	}
}

void MainWindow::emitAddNewShare(std::string str, SHA1 hash)
{
	cout << ">emit!" << str << endl;
	emit sigAddNewShare(str, hash);
	cout << "<emit!" << str << endl;
}

void MainWindow::AddNewFileInfo(void* data)
{
	FileInfoRef fi(*((FileInfoRef*)data));
	delete (FileInfoRef*)data;
	QTreeWidgetItem* rwi = treedata[fi->hash];
	if (rwi != NULL) {
		BOOST_FOREACH(const FileInfoRef& sfi, fi->files) {
			QTreeWidgetItem* srwi = new QTreeWidgetItem(rwi, 0);
			srwi->setText(0, sfi->name.c_str());
			treedata[sfi->hash] = srwi;
			JobRequest job;
			job.type = PT_QUERY_CHUNK; // refresh
			job.hash = sfi->hash;
			{
				boost::mutex::scoped_lock lock(jobs_mutex);
				JobQueue.push_back(job);
			}
		}
	}
}
