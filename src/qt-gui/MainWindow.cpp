#include "MainWindow.moc"
#include <boost/asio.hpp>

#include <iostream>

#include <QTreeWidgetItem>
#include <QFile>
#include <QUrl>

#include <QWidget>
#include <QDrag>
#include <QMimeData>
#include <QDropEvent>

#include <QFileDialog>
#include <QMessageBox>
#include <QProcess>
#include <QStringList>

#include <boost/foreach.hpp>
#include <boost/algorithm/string.hpp>

#include "QDebugStream.h"

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

	askonupdates = true;

//	debugText->setTextFormat(Qt::LogText);
	new QDebugStream(std::cout, debugText);


	this->setCentralWidget(&QWidget());

	BOOST_FOREACH(QObject* obj, children()) {
		if (QString(obj->metaObject()->className()) == "QDockWidget") {
			QDockWidget* dock = (QDockWidget*)obj;
			menu_View->addAction(dock->toggleViewAction());
			dock->setAllowedAreas(Qt::TopDockWidgetArea);
			this->addDockWidget(Qt::TopDockWidgetArea, dock);
		}
	}

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
	mainimpl.sig_new_autoupdate.connect(
		QTBOOSTER(this, MainWindow::new_autoupdate)
	);
	mainimpl.sig_download_ready.connect(
		QTBOOSTER(this, MainWindow::download_done)
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

extern string thebuildstring;

bool isbetter(std::string newstr, std::string oldstr)
{
	size_t newpos = newstr.find_last_of("-");
	size_t oldpos = oldstr.find_last_of("-");

	string newcfg = newstr.substr(0, newpos);
	string oldcfg = oldstr.substr(0, oldpos);

	if (newcfg != oldcfg) return false;

	string newver = newstr.substr(newpos+1);
	string oldver = oldstr.substr(oldpos+1);

	vector<string> newvervec;
	vector<string> oldvervec;
	boost::split(newvervec, newver, boost::is_any_of("._"));
	boost::split(oldvervec, oldver, boost::is_any_of("._"));

	for (int i = 0; i < newvervec.size() && i < oldvervec.size(); ++i) {
		int newnum = atoi(newvervec[i].c_str());
		int oldnum = atoi(oldvervec[i].c_str());
		if (oldnum > newnum) return false;
		if (oldnum < newnum) return true;
	}

	if (newvervec.size() != oldvervec.size())
		return false;

	// they are the same....
	return false;
}

void MainWindow::new_autoupdate(std::string url)
{
	size_t pos = url.find_last_of("\\/");
	string bnr = url.substr(pos+1);
	if (!isbetter(bnr, thebuildstring)) {
		return;
	}
	cout << "new autoupdate: " << url << '\n';
	if (!askonupdates)
		return;
	QMessageBox::StandardButton res = QMessageBox::question(this,
		QString("Auto Update"),
		QString::fromStdString(url),
		QMessageBox::Yes|QMessageBox::No|QMessageBox::NoToAll,
		QMessageBox::No);

	if (res == QMessageBox::NoToAll)
		askonupdates = false;
	if (res != QMessageBox::Yes)
		return;

	auto_update_url = url;
	auto_update_path = DownloadEdit->text().toStdString();
	mainimpl.slot_download_share(url, auto_update_path);

	//QDialog::
}

enum {
	RF_NEW_CONSOLE   = 1 << 0,
	RF_WAIT_FOR_EXIT = 1 << 1,
};

int RunDirect(const string& cmd, const vector<string>* args, const string& wd, int flags) {
	string command;

	STARTUPINFO si = { sizeof(si) };
	PROCESS_INFORMATION pi;

	command = cmd;

	// TODO args
	if (args) {
		for (unsigned int i = 0; i < args->size(); ++i) {
			command += " \"";
			command += (*args)[i];
			command += "\"";
		}
	}

	int res = -1;
	if (CreateProcess(
			NULL,
			TEXT((char*)command.c_str()),
			NULL,
			NULL,
			FALSE,
			(flags & RF_NEW_CONSOLE) ? CREATE_NEW_CONSOLE : 0,
			NULL, (wd=="") ? NULL : TEXT(wd.c_str()), &si, &pi))
	{
		if (flags & RF_WAIT_FOR_EXIT) {
			DWORD exitcode;
			WaitForSingleObject(pi.hProcess, INFINITE);
			if (GetExitCodeProcess(pi.hProcess, &exitcode))
				res = exitcode;
		} else
			res = 0;
		CloseHandle(pi.hProcess);
		CloseHandle(pi.hThread);
	}

	return res;
}

bool checksigniature(const std::vector<uint8>& file);

void MainWindow::download_done(std::string url)
{
	cout << "download complete: " << url << '\n';
	if (url == auto_update_url) {
		size_t pos = auto_update_url.find_last_of("\\/");
		string fname = auto_update_url.substr(pos+1) + ".exe";
		auto_update_path /= fname;
		cout << "autoupdate: " << auto_update_path << "!\n";

		{
			vector<uint8> newfile;
			uint32 todo = boost::filesystem::file_size(auto_update_path);
			newfile.resize(todo);
			ifstream istr(auto_update_path.native_file_string().c_str(), ios_base::in|ios_base::binary);
			istr.read((char*)&newfile[0], todo);
			uint32 read = istr.gcount();
			if (read != todo) {
				cout << "failed to read the new file\n";
				return;
			}
			if (!checksigniature(newfile)) {
				cout << "failed to verify the new file's signiature\n";
				return;
			}
		}

		string program = auto_update_path.native_file_string();
		vector<string> args;
		args.push_back("--runtest");
		int test = RunDirect(program, &args, "", RF_NEW_CONSOLE|RF_WAIT_FOR_EXIT);

		if (test != 0) {
			cout << "--runtest failed!\n";
			return;
		}

		args.clear();
		args.push_back("--replace");
		args.push_back(QCoreApplication::applicationFilePath().toStdString());
		int replace = RunDirect(program, &args, "", RF_NEW_CONSOLE);
		this->action_Quit->trigger();
	}
}
