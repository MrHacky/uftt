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
#include <QItemDelegate>

#include <boost/foreach.hpp>
#include <boost/algorithm/string.hpp>

#include "QDebugStream.h"

#include "../SharedData.h"
#include "../Logger.h"
#include "../SimpleBackend.h"
#include "../util/StrFormat.h"
//#include "../network/Packet.h"

#include "QtBooster.h"
#include "DialogDirectoryChooser.h"

#include <shlobj.h>

using namespace std;

string size_suffix[] =
{
	"",
	"K",
	"M",
	"G",
	"T",
	"?"
};

string size_string(double size)
{
	int suf;
	for (suf = 0; size >= 1000; ++suf)
		size /= 1024;

	if (suf > 5) suf = 5;

	//char buf[11];

	int decs = 2;

	if (size >= 100) --decs;
	if (size >= 10)  --decs;

	if (suf == 0) decs = 0;

	float fsize = size;
	std::string fstr = STRFORMAT("%%.%df %%sB", decs);
	return STRFORMAT(fstr, fsize, size_suffix[suf]);
	//snprintf(buf, 10, "%.*f", decs, size);

	//string res(buf);
	//res += size_suffix[suf];
	//return res;
};

class MyItemDelegate : public QItemDelegate
{
public:
    MyItemDelegate(QObject* parent = 0) : QItemDelegate(parent) {}
 
    QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem& option, const QModelIndex& index) const
    {
        // allow only specific column to be edited, second column in this example
        if (index.column() == 0)
            return QItemDelegate::createEditor(parent, option, index);
        return 0;
    }
};

void tester(uint64 tfx, std::string sts)
{
	cout << "update: " << sts << ": " << tfx << '\n';
}

static LogHelper loghelper;

void LogHelper::append(QString str)
{
	logAppend(str);
}

void qt_log_append(std::string str)
{
	loghelper.append(QString(str.c_str()));
}

boost::filesystem::path getFolderLocation(int nFolder)
{
	boost::filesystem::path retval;
	PIDLIST_ABSOLUTE pidlist;
	HRESULT res = SHGetSpecialFolderLocation(
		NULL,
		nFolder,
		//NULL,
		//0,
		&pidlist
	);
	if (res != S_OK) return retval;
	
	LPSTR Path = new TCHAR[MAX_PATH]; 
	if (SHGetPathFromIDList(pidlist, Path))
		retval = Path; 

	delete[] Path;
	ILFree(pidlist);
	return retval;
}

spathlist getSettingPathList() {
	spathlist result;
	boost::filesystem::path currentdir(boost::filesystem::current_path<boost::filesystem::path>());
	result.push_back(spathinfo("Current directory"      , currentdir / "uftt.dat"));
	result.push_back(spathinfo("User Documents"         , getFolderLocation(CSIDL_MYDOCUMENTS)    / "UFTT" / "uftt.dat"));
	result.push_back(spathinfo("User Application Data"  , getFolderLocation(CSIDL_APPDATA)        / "UFTT" / "uftt.dat"));
	result.push_back(spathinfo("Common Application Data", getFolderLocation(CSIDL_COMMON_APPDATA) / "UFTT" / "uftt.dat"));
	return result;
}

MainWindow::MainWindow(QTMain& mainimpl_)
: mainimpl(mainimpl_)
{
	qRegisterMetaType<std::string>("std::string");
	qRegisterMetaType<QTreeWidgetItem*>("QTreeWidgetItem*");
	qRegisterMetaType<uint64>("uint64");
	qRegisterMetaType<uint64>("uint32");
	qRegisterMetaType<SHA1C>("SHA1C");
	qRegisterMetaType<JobRequestRef>("JobRequestRef");
	qRegisterMetaType<boost::posix_time::ptime>("boost::posix_time::ptime");

	boost::filesystem::path settings_path;
	{
		spathlist spl = getSettingPathList();
		BOOST_FOREACH(const spathinfo& spi, spl) {
			if (!spi.second.empty() && boost::filesystem::exists(spi.second) && boost::filesystem::is_regular(spi.second)) {
				settings_path = spi.second;
				break;
			}
		}

		if (settings_path.empty()) {
			DialogDirectoryChooser dialog(this);
			dialog.setPaths(spl);
			int result;
			do {
				result = dialog.exec();
				if (result == QDialog::Accepted)
					settings_path = dialog.getPath();
			} while (settings_path.empty() && result == QDialog::Accepted);
			boost::filesystem::path dir = settings_path.branch_path();
			boost::filesystem::create_directories(dir);
		}
	}

	bool settingsloaded = settings.load(settings_path);

	setupUi(this);

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

	connect(&loghelper, SIGNAL(logAppend(QString)), this->debugText, SLOT(append(QString)));

	/* load/set dock layout */
	if (!settingsloaded && boost::filesystem::exists("uftt.layout")) {
		QFile layoutfile("uftt.layout");
		if (layoutfile.open(QIODevice::ReadOnly)) {
			QRect rect;
			layoutfile.read((char*)&rect, sizeof(QRect));
			settings.posx = rect.x();
			settings.posy = rect.y();
			settings.sizex = rect.width();
			settings.sizey = rect.height();
			QByteArray data = layoutfile.readAll();
			settings.dockinfo.insert(settings.dockinfo.begin(), (uint8*)data.data(), (uint8*)data.data()+data.size());
			layoutfile.remove();
		}
	}

	/* apply settings */
	if (settings.sizex != 0 && settings.sizey !=0) {
		this->resize(QSize(settings.sizex, settings.sizey));
		this->move(QPoint(settings.posx, settings.posy));
	} else
		this->resize(750, 550);

	if (settings.dockinfo.size() > 0)
		this->restoreState(QByteArray((char*)&settings.dockinfo[0],settings.dockinfo.size()));
	else {
		this->splitDockWidget (dockShares , dockWidgetDebug   , Qt::Horizontal);
		this->splitDockWidget (dockShares , dockManualConnect , Qt::Vertical  );
		this->dockManualConnect->hide();
	}

	this->DownloadEdit->setText(QString::fromStdString(settings.dl_path.native_directory_string()));
	this->actionEnableAutoupdate->setChecked(settings.autoupdate);


	/* connect Qt signals/slots */
	connect(RefreshButton, SIGNAL(clicked()), this, SLOT(RefreshButtonClicked()));
	connect(DownloadButton, SIGNAL(clicked()), this, SLOT(StartDownload()));

	//connect(SharesTree, SIGNAL(itemPressed(QTreeWidgetItem*, int)), this, SLOT(DragStart(QTreeWidgetItem*, int)));

	connect(SharesTree->getDragDropEmitter(), SIGNAL(dragMoveTriggered(QDragMoveEvent*))  , this, SLOT(onDragMoveTriggered(QDragMoveEvent*)));
	connect(SharesTree->getDragDropEmitter(), SIGNAL(dragEnterTriggered(QDragEnterEvent*)), this, SLOT(onDragEnterTriggered(QDragEnterEvent*)));
	connect(SharesTree->getDragDropEmitter(), SIGNAL(dropTriggered(QDropEvent*))          , this, SLOT(onDropTriggered(QDropEvent*)));


	listBroadcastHosts->setItemDelegate(new MyItemDelegate (listBroadcastHosts));

	ctwiu = true;
	ctwi = new QTreeWidgetItem(listBroadcastHosts);
	ctwi->setText(0, QString("[edit to add new]"));
	ctwi->setFlags(ctwi->flags() | Qt::ItemIsEditable);
	ctwiu = false;
}

void MainWindow::on_listBroadcastHosts_itemChanged( QTreeWidgetItem * item, int column)
{
	if (ctwiu) return;
	if (item == ctwi) {
		ctwiu = true;
		ctwi = new QTreeWidgetItem(listBroadcastHosts);
		ctwi->setText(0, QString("[edit to add new]"));
		ctwi->setFlags(ctwi->flags() | Qt::ItemIsEditable);
		ctwiu = false;
	}
	if (item->text(0) == "") {
		delete item;
		return;
	}
}

void MainWindow::closeEvent(QCloseEvent * evnt)
{
	/* put stuff back into settings */
	settings.posx = this->pos().x();
	settings.posy = this->pos().y();
	settings.sizex = this->size().width();
	settings.sizey = this->size().height();

	settings.dockinfo.clear();
	QByteArray data = saveState();
	settings.dockinfo.insert(settings.dockinfo.begin(), (uint8*)data.data(), (uint8*)data.data()+data.size());

	settings.dl_path = DownloadEdit->text().toStdString();

	/* and save them */
	settings.save();

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
	backend->slot_refresh_shares();
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

	QTreeWidgetItem* twi = new QTreeWidgetItem(listTasks);
	twi->setText(0, name);
	//twi->setText(1, QString::fromStdString(STRFORMAT("%d", 0)));
	//twi->setText(3, QString("Starting..."));
	boost::posix_time::ptime starttime = boost::posix_time::second_clock::universal_time();
	boost::function<void(uint64, std::string, uint32)> handler = boost::bind(
		QTBOOSTER(this, MainWindow::download_progress),
		//boost::bind(&MainWindow::download_progress, this, _1, _2, _3),
		twi, _1, _2, starttime, _3);
	handler(0, "Starting", 0);
//		boost::bind(&MainWindow::download_progress, this&tester, _1, _2);
	backend->slot_download_share(name.toStdString(), fs::path(DownloadEdit->text().toStdString()), handler);
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

void MainWindow::download_progress(QTreeWidgetItem* twi, uint64 tfx, std::string sts, boost::posix_time::ptime starttime, uint32 queue)
{
	boost::posix_time::ptime curtime = boost::posix_time::second_clock::universal_time();
	boost::posix_time::time_duration elapsed = curtime-starttime;

	twi->setText(1, QString::fromStdString(size_string(tfx)));
	twi->setText(2, QString::fromStdString(boost::posix_time::to_simple_string(elapsed)));
	twi->setText(3, QString::fromStdString(sts));
	twi->setText(4, QString::fromStdString(STRFORMAT("%d", queue)));
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
	backend->slot_add_local_share(path.leaf(), path);
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

	for (uint i = 0; i < newvervec.size() && i < oldvervec.size(); ++i) {
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
	if (!settings.autoupdate)
		return;
	size_t pos = url.find_last_of("\\/");
	string bnr = url.substr(pos+1);
	if (!isbetter(bnr, thebuildstring)) {
		return;
	}
	cout << "new autoupdate: " << url << '\n';
	static bool dialogshowing = false;
	if (dialogshowing) return;
	dialogshowing = true;
	QMessageBox::StandardButton res = QMessageBox::question(this,
		QString("Auto Update"),
		QString::fromStdString(url),
		QMessageBox::Yes|QMessageBox::No|QMessageBox::NoToAll,
		QMessageBox::No);
	dialogshowing = false;

	if (res == QMessageBox::NoToAll)
		this->actionEnableAutoupdate->setChecked(false);
	if (res != QMessageBox::Yes)
		return;

	auto_update_url = url;
	auto_update_path = DownloadEdit->text().toStdString();
	boost::function<void(uint64, std::string, uint32)> handler = boost::bind(&tester, _1, _2);
	backend->slot_download_share(url, auto_update_path, handler);

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
	} else
		cout << "CreateProcess failed\n";

	return res;
}

bool checksigniature(const std::vector<uint8>& file);

void MainWindow::download_done(std::string url)
{
	cout << "download complete: " << url << '\n';
	if (url == auto_update_url) {
		size_t pos = auto_update_url.find_last_of("\\/");
		string bname = auto_update_url.substr(pos+1);
		string fname = bname + ".exe";
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
		args.push_back(bname);
		int test = RunDirect(program, &args, "", RF_NEW_CONSOLE|RF_WAIT_FOR_EXIT);

		if (test != 0) {
			cout << "--runtest failed!\n";
			return;
		}

		args.clear();
		args.push_back("--replace");
		args.push_back(boost::filesystem::path(QCoreApplication::applicationFilePath().toStdString()).native_file_string());
		int replace = RunDirect(program, &args, "", RF_NEW_CONSOLE);
		this->action_Quit->trigger();
	}
}

void MainWindow::SetBackend(SimpleBackend* be)
{
	backend = be;

	backend->sig_new_share.connect(
		QTBOOSTER(this, MainWindow::addSimpleShare)
	);
	backend->sig_new_autoupdate.connect(
		QTBOOSTER(this, MainWindow::new_autoupdate)
	);
	backend->sig_download_ready.connect(
		QTBOOSTER(this, MainWindow::download_done)
	);
	backend->sig_new_upload.connect(
		QTBOOSTER(this, MainWindow::new_upload)
	);
}

void MainWindow::on_buttonManualQuery_clicked()
{
	backend->do_manual_query(editManualQuery->text().toStdString());
}

void MainWindow::on_buttonManualPublish_clicked()
{
	backend->do_manual_publish(editManualPublish->text().toStdString());
}

void MainWindow::on_actionEnableAutoupdate_toggled(bool value)
{
	settings.autoupdate = value;
}

void MainWindow::on_buttonClearCompletedTasks_clicked()
{
	for(int i = this->listTasks->topLevelItemCount(); i > 0; --i) {
		QTreeWidgetItem* twi = this->listTasks->topLevelItem(i-1);
		if (twi->text(3) == "Completed")
			delete twi;
	}
}

void MainWindow::new_upload(std::string name, int num)
{
	QTreeWidgetItem* twi = new QTreeWidgetItem(listTasks);
	twi->setText(0, QString::fromStdString(name));
	//twi->setText(1, QString::fromStdString(STRFORMAT("%d", 0)));
	//twi->setText(3, QString("Starting..."));
	boost::posix_time::ptime starttime = boost::posix_time::second_clock::universal_time();
	boost::function<void(uint64, std::string, uint32)> handler = boost::bind(
		QTBOOSTER(this, MainWindow::download_progress),
		//boost::bind(&MainWindow::download_progress, this, _1, _2, _3),
		twi, _1, _2, starttime, _3);
	backend->slot_attach_progress_handler(num, handler);
	handler(0, "Starting", 0);
}
