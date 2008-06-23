#include "MainWindow.moc"

#include <iostream>

#include <QTreeWidgetItem>
#include <QFile>
#include <QUrl>

#include <QWidget>
#include <QDrag>
#include <QMimeData>
#include <QDropEvent>

#include <QFileDialog>

#include <boost/foreach.hpp>

#include "../SharedData.h"
#include "../Logger.h"
//#include "../network/Packet.h"

#include "QtBooster.h"

using namespace std;

static LogHelper loghelper;

void LogHelper::append(QString str)
{
	logAppend(str);
}

void qt_log_append(std::string str)
{
	loghelper.append(QString(str.c_str()));
}

MainWindow::MainWindow(QTMain& mainimpl_)
: mainimpl(mainimpl_)
{
	qRegisterMetaType<std::string>("std::string");
	qRegisterMetaType<SHA1C>("SHA1C");
	qRegisterMetaType<JobRequestRef>("JobRequestRef");

	setupUi(this);
	this->setCentralWidget(&QWidget());

	buttonAdd2->hide();
	buttonAdd3->hide();

	this->resize(550,350);

	connect(&loghelper, SIGNAL(logAppend(QString)), this->debugText, SLOT(append(QString)));
	// load layout from file
	QFile layoutfile("uftt.lyt");
	if (false && layoutfile.open(QIODevice::ReadOnly)) {
		QByteArray data = layoutfile.readAll();
		if (data.size() > 0)
			restoreState(data);
		layoutfile.close();
	} else
		this->dockWidgetDebug->hide();

	connect(RefreshButton, SIGNAL(clicked()), this, SLOT(RefreshButtonClicked()));
	connect(DownloadButton, SIGNAL(clicked()), this, SLOT(StartDownload()));

	connect(SharesTree, SIGNAL(itemPressed(QTreeWidgetItem*, int)), this, SLOT(DragStart(QTreeWidgetItem*, int)));
	//connect(this, SIGNAL(sigAddNewShare()), this, SLOT(AddNewShare()));

	//connect(treeWidget->getDragDropEmitter(), SIGNAL(dragMoveTriggered(QDragMoveEvent*))  , this, SLOT(onDragMoveTriggered(QDragMoveEvent*)));
	//connect(treeWidget->getDragDropEmitter(), SIGNAL(dragEnterTriggered(QDragEnterEvent*)), this, SLOT(onDragEnterTriggered(QDragEnterEvent*)));
	//connect(treeWidget->getDragDropEmitter(), SIGNAL(dropTriggered(QDropEvent*))          , this, SLOT(onDropTriggered(QDropEvent*)));

	connect(SharesTree->getDragDropEmitter(), SIGNAL(dragMoveTriggered(QDragMoveEvent*))  , this, SLOT(onDragMoveTriggered(QDragMoveEvent*)));
	connect(SharesTree->getDragDropEmitter(), SIGNAL(dragEnterTriggered(QDragEnterEvent*)), this, SLOT(onDragEnterTriggered(QDragEnterEvent*)));
	connect(SharesTree->getDragDropEmitter(), SIGNAL(dropTriggered(QDropEvent*))          , this, SLOT(onDropTriggered(QDropEvent*)));

	mainimpl.sig_new_share.connect(
		QTBOOSTER(this, MainWindow::addSimpleShare)
	);

	mainimpl.slot_refresh_shares();
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
	/*
	JobRequestQueryServersRef job(new JobRequestQueryServers());
	{
		boost::mutex::scoped_lock lock(jobs_mutex);
		JobQueue.push_back(job);
	}*/
	mainimpl.slot_refresh_shares();
}

void MainWindow::AddNewServer()
{
	LOG("TODO?: AddNewServer()");
}

void MainWindow::addSimpleShare(std::string sharename)
{
	QString qsharename = QString::fromStdString(sharename);

	QList<QTreeWidgetItem*> fres = SharesTree->findItems(qsharename, 0);

	if (fres.empty()) {
		QTreeWidgetItem* rwi = new QTreeWidgetItem(SharesTree, 0);
		rwi->setText(0, qsharename);
	}	
}

void MainWindow::AddNewShare(std::string str, SHA1C hash)
{
	QTreeWidgetItem* rwi = treedata[hash];
	if (rwi == NULL) {
		rwi = new QTreeWidgetItem(SharesTree, 0);
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
	QTreeWidgetItem* rwi = SharesTree->currentItem();
	if (!rwi)
		return;
	QString name = rwi->text(0);
	mainimpl.slot_download_share(name.toStdString(), fs::path(DownloadEdit->text().toStdString()));
	return;
	SHA1C hash;
	typedef std::pair<SHA1C, QTreeWidgetItem*> pairtype;
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

void MainWindow::addLocalShare(std::string url)
{
	boost::filesystem::path path = url;
	mainimpl.slot_add_local_share(path.leaf(), path);
}

void MainWindow::onDropTriggered(QDropEvent* evt)
{
	LOG("try=" << evt->mimeData()->text().toStdString());
	evt->acceptProposedAction();

	BOOST_FOREACH(const QUrl & url, evt->mimeData()->urls()) {
		// weird stuff here, next line gives a weird error popup in release mode,
		// but the program continues just fine...
		string str(url.toLocalFile().toStdString());
		// next one give no popup
		// string str(url.toLocalFile().toAscii().data());

		if (!str.empty()) {
			this->addLocalShare(str);
			/*
			FileInfoRef fi(new FileInfo(str));
			addFileInfo(*fi);
			
			ShareInfo fs(fi);
			fs.path = str;
			{
				boost::mutex::scoped_lock lock(shares_mutex);
				MyShares.push_back(fs);
			}
			*/
		}
	}
}

void MainWindow::onDragEnterTriggered(QDragEnterEvent* evt)
{
	if (evt->mimeData()->hasUrls())
		evt->acceptProposedAction();
}

void MainWindow::onDragMoveTriggered(QDragMoveEvent* evt)
{
	QWidget::dragMoveEvent(evt);
}

void MainWindow::on_buttonAdd1_clicked()
{
	QString directory;
	directory = QFileDialog::getExistingDirectory(this,
		tr("Choose directory to share"),
		"");
	if (!directory.isEmpty())
		this->addLocalShare(directory.toStdString());
}

void MainWindow::on_buttonAdd2_clicked()
{
	QString directory;
	directory = QFileDialog::getExistingDirectory(this,
		tr("Choose directory to share"),
		"",0);
	if (!directory.isEmpty())
		this->addLocalShare(directory.toStdString());
}

void MainWindow::on_buttonAdd3_clicked()
{
	QString directory;
	directory = QFileDialog::getOpenFileName (this, tr("Choose file to share"),
		"",
		tr("any (*)"));
	if (!directory.isEmpty())
		this->addLocalShare(directory.toStdString());
}

void MainWindow::on_buttonBrowse_clicked()
{
	QString directory;
	directory = QFileDialog::getExistingDirectory(this,
		tr("Choose directory to share"),
		DownloadEdit->text());
	if (!directory.isEmpty())
		this->DownloadEdit->setText(QString::fromStdString(boost::filesystem::path(directory.toStdString()).native_directory_string()));
}
