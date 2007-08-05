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
	connect(DownloadButton, SIGNAL(clicked()), this, SLOT(StartDownload()));

	connect(this, SIGNAL(sigAddNewServer()), this, SLOT(AddNewServer()));
	connect(this, SIGNAL(sigAddNewShare(std::string, SHA1)), this, SLOT(AddNewShare(std::string, SHA1)));
	connect(this, SIGNAL(sigAddNewFileInfo(void*)), this, SLOT(AddNewFileInfo(void*)));
	connect(OthersSharesTree, SIGNAL(itemPressed(QTreeWidgetItem*, int)), this, SLOT(DragStart(QTreeWidgetItem*, int)));
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
	dirdata[fi->hash] = fi;
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

void MainWindow::DragStart(QTreeWidgetItem* rwi, int col)
{
	if (rwi != NULL && col == 0) {
		QDrag *drag = new QDrag(this);
		QMimeData *mimeData = new QMimeData;

		mimeData->setText(rwi->text(0));
		drag->setMimeData(mimeData);
		//drag->setPixmap(iconPixmap);

		Qt::DropAction dropAction = drag->start();
		// TODO: figure this out...
	}

}

void MainWindow::StartDownload()
{
	QTreeWidgetItem* rwi = OthersSharesTree->currentItem();
	SHA1 hash;
	typedef std::pair<SHA1, QTreeWidgetItem*> pairtype;
	BOOST_FOREACH(const pairtype & ip, treedata) {
		if (ip.second == rwi)
			hash = ip.first;
	}
	FileInfoRef fi = dirdata[hash];
	if (!fi) {
		cout << "hash not found!" << endl;
		return;
	}
	StartDownload(fi, fs::path(DownloadEdit->text().toStdString()) / rwi->text(0).toStdString());
	//cout << "Start Download!" <<  << endl;
}

void MainWindow::StartDownload(FileInfoRef fi, const fs::path& path)
{
	fi = dirdata[fi->hash];
	cout << "fp:" << path << endl;
	if (fi->files.size() > 0) {
		create_directory(path);
	}
	BOOST_FOREACH(const FileInfoRef& fir, fi->files) {
		StartDownload(fir, path / fir->name);
	}
}
