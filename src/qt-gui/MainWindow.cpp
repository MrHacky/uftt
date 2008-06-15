#include "MainWindow.moc"

#include <iostream>

#include <QTreeWidgetItem>
#include <QFile>

#include <boost/foreach.hpp>

#include "../SharedData.h"
#include "../Logger.h"
#include "../network/Packet.h"

using namespace std;

static LogHelper loghelper;

void LogHelper::append(QString str)
{
	emit logAppend(str);
}

void qt_log_append(std::string str)
{
	loghelper.append(QString(str.c_str()));
}

MainWindow::MainWindow()
{
	qRegisterMetaType<std::string>("std::string");
	qRegisterMetaType<SHA1>("SHA1");
	qRegisterMetaType<JobRequestRef>("JobRequestRef");

	setupUi(this);

	connect(&loghelper, SIGNAL(logAppend(QString)), this->debugText, SLOT(append(QString)));
	// load layout from file
	QFile layoutfile("uftt.lyt");
	if (layoutfile.open(QIODevice::ReadOnly)) {
		QByteArray data = layoutfile.readAll();
		if (data.size() > 0)
			restoreState(data);
		layoutfile.close();
	}

	connect(RefreshButton, SIGNAL(clicked()), this, SLOT(RefreshButtonClicked()));
	connect(DownloadButton, SIGNAL(clicked()), this, SLOT(StartDownload()));

	connect(OthersSharesTree, SIGNAL(itemPressed(QTreeWidgetItem*, int)), this, SLOT(DragStart(QTreeWidgetItem*, int)));
	//connect(this, SIGNAL(sigAddNewShare()), this, SLOT(AddNewShare()));
}

void MainWindow::closeEvent(QCloseEvent * evnt)
{
	// save layout to file
	QFile layoutfile("uftt.lyt");
	if (layoutfile.open(QIODevice::WriteOnly)) {
		QByteArray data = saveState();
		if (data.size() > 0)
			layoutfile.write(data);
		layoutfile.close();
	}
	QWidget::closeEvent(evnt);
}

MainWindow::~MainWindow()
{
}

void MainWindow::RefreshButtonClicked()
{
	JobRequestQueryServersRef job(new JobRequestQueryServers());
	{
		boost::mutex::scoped_lock lock(jobs_mutex);
		JobQueue.push_back(job);
	}
}

void MainWindow::AddNewServer()
{
	LOG("TODO?: AddNewServer()");
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
		JobRequestTreeDataRef job(new JobRequestTreeData());
		job->hash = hash;
		{
			boost::mutex::scoped_lock lock(jobs_mutex);
			JobQueue.push_back(job);
		}
	}
}

void MainWindow::NewTreeInfo(JobRequestRef basejob)
{
	assert(basejob->type() == JRT_TREEDATA);
	JobRequestTreeDataRef job = boost::static_pointer_cast<JobRequestTreeData>(basejob);

	FileInfoRef fi = dirdata[job->hash];
	if (!fi) {
		fi = FileInfoRef(new FileInfo());
		fi->hash = job->hash;
		dirdata[fi->hash] = fi;
	}

	if (job->chunkcount == 0) {
		fi->attrs &= ~FATTR_DIR;
		return; // is a blob, not a tree
	}
	fi->attrs |= FATTR_DIR;

	QTreeWidgetItem* rwi = treedata[fi->hash];
	if (rwi != NULL) {
		BOOST_FOREACH(const JobRequestTreeData::child_info& ci, job->children) {
			QTreeWidgetItem* srwi = new QTreeWidgetItem(rwi, 0);
			srwi->setText(0, ci.name.c_str());
			treedata[ci.hash] = srwi;
			JobRequestTreeDataRef newjob(new JobRequestTreeData());
			newjob->hash = ci.hash;
			{
				boost::mutex::scoped_lock lock(jobs_mutex);
				JobQueue.push_back(newjob);
			}
			FileInfoRef nfi = dirdata[ci.hash];
			if (!nfi) {
				nfi = FileInfoRef(new FileInfo());
				nfi->hash = ci.hash;
				dirdata[nfi->hash] = nfi;
			}
			nfi->name = ci.name;
			fi->files.push_back(nfi);
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
		LOG("hash not found!");
		return;
	}
	StartDownload(fi, fs::path(DownloadEdit->text().toStdString()) / rwi->text(0).toStdString());
	//cout << "Start Download!" <<  << endl;
}

void MainWindow::StartDownload(FileInfoRef fi, const fs::path& path)
{
	LOG("fp:" << path);
	fi = dirdata[fi->hash];
	if (!fi) {
		LOG("hash not found!");
		return;
	}
	LOG("fa:" << fi->attrs);
	if (fi->attrs & FATTR_DIR) {
		create_directory(path);
		BOOST_FOREACH(const FileInfoRef& fir, fi->files) {
			StartDownload(fir, path / fir->name);
		}
	} else {
		JobRequestBlobDataRef job(new JobRequestBlobData());
		job->hash = fi->hash;
		job->fpath = path;
		{
			boost::mutex::scoped_lock lock(jobs_mutex);
			JobQueue.push_back(job);
		}
	}
}
