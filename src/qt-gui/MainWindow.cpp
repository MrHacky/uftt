#include "MainWindow.moc"
#include <boost/asio.hpp>

#include <iostream>
#include <fstream>

#include <QDesktopServices>
#include <QDir>
#include <QDrag>
#include <QDropEvent>
#include <QFile>
#include <QFileDialog>
#include <QHeaderView>
#include <QItemDelegate>
#include <QMessageBox>
#include <QMimeData>
#include <QProcess>
#include <QStringList>
#include <QTimer>
#include <QTreeWidgetItem>
#include <QUrl>
#include <QWidget>

#include <boost/foreach.hpp>

#include "QDebugStream.h"
#include "QToggleHeaderAction.h"

#include "../SimpleBackend.h"
#include "../Platform.h"
#include "../AutoUpdate.h"
#include "../util/StrFormat.h"

#include "DialogDirectoryChooser.h"

using namespace std;

class MyItemDelegate : public QItemDelegate
{
public:
    MyItemDelegate(QObject* parent = 0) : QItemDelegate(parent) {}

    QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem& option, const QModelIndex& index) const
    {
        // allow only specific column to be edited, first column in this example
        if (index.column() == 0)
            return QItemDelegate::createEditor(parent, option, index);
        return 0;
    }
};

MainWindow::MainWindow(UFTTSettings& settings_)
: settings(settings_)
{
	setupUi(this);

	if (string("QWindowsXPStyle") == this->style()->metaObject()->className()) {
		// this style has invisible separators sometimes,
		// that looks horrible, so make them visible
		const char sepstyle[] =
			"QMainWindow::Separator:horizontal {"
			"  border-top: 1px solid palette(light);"
			"  border-bottom: 2px solid qlineargradient(spread:pad, x1:0, y1:0, x2:0, y2:1, stop:0 palette(dark), stop:0.495 palette(dark), stop:0.505 palette(darker), stop:1 palette(darker));"
			"}"
			""
			"QMainWindow::Separator:vertical {"
			"  border-left: 1px solid palette(light);"
			"  border-right: 2px solid qlineargradient(spread:pad, x1:0, y1:0, x2:1, y2:0, stop:0 palette(dark), stop:0.495 palette(dark), stop:0.505 palette(darker), stop:1 palette(darker));"
			"}";
		this->setStyleSheet(QString(sepstyle));
	}

	// setup debug stream
	new QDebugStream(std::cout, debugText);

	{
		QWidget* cw = new QWidget();
		this->setCentralWidget(cw);
		delete cw;
	}

	this->setDockOptions(QMainWindow::DockOptions(QMainWindow::AllowNestedDocks|QMainWindow::AllowTabbedDocks|QMainWindow::AnimatedDocks|QMainWindow::VerticalTabs));

	BOOST_FOREACH(QObject* obj, children()) {
		if (QString(obj->metaObject()->className()) == "QDockWidget") {
			QDockWidget* dock = (QDockWidget*)obj;
			menu_View->addAction(dock->toggleViewAction());
			dock->setAllowedAreas(Qt::TopDockWidgetArea);
			dock->setFeatures(QDockWidget::DockWidgetClosable | QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
			this->addDockWidget(Qt::TopDockWidgetArea, dock);
		}
	}

	buttonAdd2->hide();
	buttonAdd3->hide();

	/* load/set dock layout */
	if (!settings.loaded && ext::filesystem::exists("uftt.layout")) {
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

	/* apply size settings */
	if (settings.sizex != 0 && settings.sizey !=0) {
		this->resize(QSize(settings.sizex, settings.sizey));
		this->move(QPoint(settings.posx, settings.posy));
	} else
		this->resize(750, 550);

	/* apply dock layout */
	if (settings.dockinfo.size() > 0)
		this->restoreState(QByteArray((char*)&settings.dockinfo[0],settings.dockinfo.size()));
	else {
		this->splitDockWidget (dockShares   , dockTaskList      , Qt::Horizontal);
		this->splitDockWidget (dockTaskList , dockWidgetDebug   , Qt::Vertical  );
		this->splitDockWidget (dockShares   , dockManualConnect , Qt::Vertical  );
		this->dockManualConnect->hide();
		#ifdef NDEBUG
			this->dockWidgetDebug->hide();
		#endif
	}

	/* set sharelist layout */
	//this->listShares->header()->moveSection(2+0,0);
	//this->listShares->header()->moveSection(1+1,1);
	//this->listShares->header()->moveSection(0+2,2);
	//this->listShares->header()->moveSection(3+0,3);
	this->listShares->hideColumn(2);
	this->listShares->hideColumn(3);
	QToggleHeaderAction::addActions(this->listShares);
	QToggleHeaderAction::addActions(this->listTasks);

	if (!ext::filesystem::exists(settings.dl_path)) {
		boost::filesystem::path npath(QDir::tempPath().toStdString());
		if (ext::filesystem::exists(npath))
			settings.dl_path = npath;
	}

	this->DownloadEdit->setText(QString::fromStdString(settings.dl_path.native_directory_string()));
	this->actionEnableAutoupdate->setChecked(settings.autoupdate);
	this->setUpdateInterval(settings.webupdateinterval);

	//connect(listShares, SIGNAL(itemPressed(QTreeWidgetItem*, int)), this, SLOT(DragStart(QTreeWidgetItem*, int)));

	connect(listShares->getDragDropEmitter(), SIGNAL(dragMoveTriggered(QDragMoveEvent*))  , this, SLOT(onDragMoveTriggered(QDragMoveEvent*)));
	connect(listShares->getDragDropEmitter(), SIGNAL(dragEnterTriggered(QDragEnterEvent*)), this, SLOT(onDragEnterTriggered(QDragEnterEvent*)));
	connect(listShares->getDragDropEmitter(), SIGNAL(dropTriggered(QDropEvent*))          , this, SLOT(onDropTriggered(QDropEvent*)));


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

	QWidget::closeEvent(evnt);
}

MainWindow::~MainWindow()
{
}

void MainWindow::on_buttonRefresh_clicked()
{
	backend->slot_refresh_shares();
}

void MainWindow::addSimpleShare(std::string sharename)
{
	std::string url = sharename;
	std::string shareurl = url;
	size_t colonpos = shareurl.find_first_of(":");
	std::string proto = shareurl.substr(0, colonpos);
	uint32 version = atoi(proto.substr(6).c_str());
	shareurl.erase(0, proto.size()+3);
	size_t slashpos = shareurl.find_first_of("\\/");
	std::string host = shareurl.substr(0, slashpos);
	std::string share = shareurl.substr(slashpos+1);

	QString qshare = QString::fromStdString(share);
	QString qproto = QString::fromStdString(proto);
	QString qhost  = QString::fromStdString(host);
	QString qurl   = QString::fromStdString(url);

	QList<QTreeWidgetItem*> fres = listShares->findItems(qshare, 0, 0);

	bool found = false;
	BOOST_FOREACH(QTreeWidgetItem* twi, fres) {
		if (twi->text(1) == qhost && twi->text(0) == qshare) {
			found = true;
			QString qoprot = twi->text(2);
			uint32 over = atoi(qoprot.toStdString().substr(6).c_str());
			if (over < version) {
				twi->setText(2, qproto);
				twi->setText(3, qurl);
			}
		}
	}

	if (!found) {
		QTreeWidgetItem* rwi = new QTreeWidgetItem(listShares, 0);
		rwi->setText(0, qshare);
		rwi->setText(1, qhost);
		rwi->setText(2, qproto);
		rwi->setText(3, qurl);
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

		// Qt::DropAction dropAction = drag->start();
		// TODO: figure this out...
	}

}

void MainWindow::on_buttonDownload_clicked()
{
	boost::filesystem::path dlpath = DownloadEdit->text().toStdString();
	if (!ext::filesystem::exists(dlpath)) {
		QMessageBox::information (this, "Download Failed", "Select a valid download directory first");
		return;
	}
	QTreeWidgetItem* rwi = listShares->currentItem();
	if (!rwi)
		return;
	QString url = rwi->text(3);

	QTreeWidgetItem* twi = new QTreeWidgetItem(listTasks);
	twi->setText(0, url);

	boost::posix_time::ptime starttime = boost::posix_time::second_clock::universal_time();
	boost::function<void(uint64, std::string, uint32, uint64)> handler =
		marshaller.wrap(boost::bind(&MainWindow::download_progress, this, twi, _1, _2, starttime, _3, _4));
	handler(0, "Starting", 0, 0);

	backend->slot_download_share(url.toStdString(), dlpath, handler);
}


void MainWindow::download_progress(QTreeWidgetItem* twi, uint64 tfx, std::string sts, boost::posix_time::ptime starttime, uint32 queue, uint64 total)
{
	boost::posix_time::ptime curtime = boost::posix_time::second_clock::universal_time();
	boost::posix_time::time_duration elapsed = curtime-starttime;

	twi->setText(1, QString::fromStdString(StrFormat::bytes(tfx)));
	twi->setText(2, QString::fromStdString(boost::posix_time::to_simple_string(elapsed)));
	twi->setText(3, QString::fromStdString(sts));
	twi->setText(4, QString::fromStdString(STRFORMAT("%d", queue)));
	twi->setText(5, QString::fromStdString(StrFormat::bytes(total)));
	if (elapsed.total_seconds() > 0)
		twi->setText(6, QString::fromStdString(STRFORMAT("%s\\s", StrFormat::bytes(tfx/elapsed.total_seconds()))));
	if (total > 0 && total >= tfx && tfx > 0)
		twi->setText(7, QString::fromStdString(
		boost::posix_time::to_simple_string(
			boost::posix_time::time_duration(
				boost::posix_time::seconds(
					((total-tfx) * elapsed.total_seconds() / tfx)
					)
				)
			)
		));

	if (sts == "Completed") {
		string name = twi->text(0).toStdString();
		if (name.substr(0, 12) == "Autoupdate: ") {
			name.erase(0, 12);
			download_done(name);
		}
	}
}

void MainWindow::addLocalShare(std::string url)
{
	boost::filesystem::path path = url;
	if (path.leaf() == ".") path.remove_leaf(); // linux thingy
	backend->slot_add_local_share(path.leaf(), path);
}

void MainWindow::onDropTriggered(QDropEvent* evt)
{
	//LOG("try=" << evt->mimeData()->text().toStdString());
	evt->acceptProposedAction();

	BOOST_FOREACH(const QUrl & url, evt->mimeData()->urls()) {
		string str(url.toLocalFile().toStdString());

		if (!str.empty()) {
			this->addLocalShare(str);
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

void MainWindow::new_autoupdate(std::string url, std::string build, bool fromweb)
{
	if (!fromweb && !settings.autoupdate)
		return;
	if (!fromweb) {
		size_t pos = url.find_last_of("\\/");
		build = url.substr(pos+1);
	}

	if (!AutoUpdater::isBuildBetter(build, thebuildstring)) {
		cout << "ignoring update: " << build << " @ " << url << '\n';
		return;
	}
	cout << "new autoupdate: " << build << " @ " << url << '\n';
	static bool dialogshowing = false;
	if (dialogshowing) return;
	dialogshowing = true;
	QMessageBox::StandardButton res = QMessageBox::question(this,
		QString("Auto Update"),
		QString::fromStdString(
			"Build: " + build + '\r' + '\n' +
			"URL: " + url
		),
		QMessageBox::Yes|QMessageBox::No|(fromweb ? QMessageBox::NoButton : QMessageBox::NoToAll),
		QMessageBox::No);
	dialogshowing = false;

	if (res == QMessageBox::NoToAll)
		this->actionEnableAutoupdate->setChecked(false);
	if (res != QMessageBox::Yes)
		return;

	if (!fromweb) {
		auto_update_url = url;
		auto_update_path = DownloadEdit->text().toStdString();

		QTreeWidgetItem* twi = new QTreeWidgetItem(listTasks);
		twi->setText(0, QString::fromStdString(string() + "Autoupdate: " + url));

		boost::posix_time::ptime starttime = boost::posix_time::second_clock::universal_time();
		boost::function<void(uint64, std::string, uint32, uint64)> handler =
			marshaller.wrap(boost::bind(&MainWindow::download_progress, this, twi, _1, _2, starttime, _3, _4));
		handler(0, "Starting", 0, 0);
		backend->slot_download_share(url, auto_update_path, handler);
	} else {
		backend->do_download_web_update(url,
			marshaller.wrap(boost::bind(&MainWindow::cb_web_download_done, this, _1, build, _2))
		);
	}
}

void MainWindow::download_done(std::string url)
{
	cout << "download complete: " << url << " (" << auto_update_url << ")\n";
	if (url == auto_update_url) {
		size_t pos = auto_update_url.find_last_of("\\/");
		string bname = auto_update_url.substr(pos+1);
		string fname = bname;
		if (bname.find("win32") != string::npos) fname += ".exe";
		if (bname.find("-deb-") != string::npos) fname += ".deb.signed";
		auto_update_path /= fname;
		cout << "autoupdate: " << auto_update_path << "!\n";

		this->doSelfUpdate(bname, auto_update_path);
	}
}

void MainWindow::SetBackend(SimpleBackend* be)
{
	backend = be;

	backend->sig_new_share.connect(
		marshaller.wrap(boost::bind(&MainWindow::addSimpleShare, this, _1))
	);
	backend->sig_new_autoupdate.connect(
		marshaller.wrap(boost::bind(&MainWindow::new_autoupdate, this, _1, "", false))
	);
	backend->sig_new_upload.connect(
		marshaller.wrap(boost::bind(&MainWindow::new_upload, this, _1, _2))
	);
}

void MainWindow::on_buttonManualQuery_clicked()
{
	try {
		backend->do_manual_query(editManualQuery->text().toStdString());
	} catch (std::exception& e) {
		cout << "exception: " << e.what() << '\n';
	}
}

void MainWindow::on_buttonManualPublish_clicked()
{
	try {
		backend->do_manual_publish(editManualPublish->text().toStdString());
	} catch (std::exception& e) {
		cout << "exception: " << e.what() << '\n';
	}
}

void MainWindow::on_actionEnableAutoupdate_toggled(bool value)
{
	settings.autoupdate = value;
}

void MainWindow::on_actionEnableGlobalPeerfinder_toggled(bool enabled)
{
	backend->do_set_peerfinder_enabled(enabled);
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

	boost::posix_time::ptime starttime = boost::posix_time::second_clock::universal_time();
	boost::function<void(uint64, std::string, uint32, uint64)> handler =
		marshaller.wrap(boost::bind(&MainWindow::download_progress, this, twi, _1, _2, starttime, _3, _4));

	handler(0, "Starting", 0, 0);
	backend->slot_attach_progress_handler(num, handler);
}

void MainWindow::on_actionCheckForWebUpdates_triggered()
{
	settings.lastupdate = boost::posix_time::second_clock::universal_time();
	backend->do_check_for_web_updates(
		marshaller.wrap(boost::bind(&MainWindow::new_autoupdate, this, _2, _1, true))
	);
}

void MainWindow::cb_web_download_done(const boost::system::error_code& err, const std::string& build, boost::shared_ptr<boost::asio::http_request> req)
{
	if (err) {
		cout << "Web download failed: " << err.message() << '\n';
	} else {
		boost::filesystem::path temp_path = DownloadEdit->text().toStdString();
		temp_path /= (build + ".exe");

		ofstream tfile(temp_path.native_file_string().c_str(), ios_base::out|ios_base::binary);
		if (!req->getContent().empty())
			tfile.write((char*)&(req->getContent()[0]), req->getContent().size());
		tfile.close();

		this->doSelfUpdate(build, temp_path);
	}
}

void MainWindow::on_actionUpdateNever_toggled(bool on)
{
	if (on) setUpdateInterval(0);
}

void MainWindow::on_actionUpdateDaily_toggled(bool on)
{
	if (on) setUpdateInterval(1);
}

void MainWindow::on_actionUpdateWeekly_toggled(bool on)
{
	if (on) setUpdateInterval(2);
}

void MainWindow::on_actionUpdateMonthly_toggled(bool on)
{
	if (on) setUpdateInterval(3);
}

void MainWindow::setUpdateInterval(int i)
{
	actionUpdateNever->setChecked(i == 0);
	actionUpdateDaily->setChecked(i == 1);
	actionUpdateWeekly->setChecked(i == 2);
	actionUpdateMonthly->setChecked(i == 3);

	settings.webupdateinterval = i;
}

void MainWindow::check_autoupdate_interval()
{
	QTimer::singleShot(1000 * 60 * 60, this, SLOT(check_autoupdate_interval()));
	//QTimer::singleShot(1000 * 3, this, SLOT(check_autoupdate_interval()));

	if (settings.webupdateinterval == 0) return;
	int minhours = 30 * 24;
	if (settings.webupdateinterval == 1) minhours = 1 * 24;
	if (settings.webupdateinterval == 2) minhours = 7 * 24;

	boost::posix_time::ptime curtime = boost::posix_time::second_clock::universal_time();

	int lasthours = (curtime - settings.lastupdate).hours();
	//int lasthours = (curtime - settings.lastupdate).total_seconds();

	std::cout << STRFORMAT("last update check: %dh ago (%dh)\n", lasthours, minhours);

	if (lasthours > minhours) {
		actionCheckForWebUpdates->trigger();
		//on_actionCheckForWebUpdates_triggered();
	}
}

void MainWindow::onshow()
{
	this->actionEnableGlobalPeerfinder->setChecked(settings.enablepeerfinder);
	check_autoupdate_interval();
}

void MainWindow::doSelfUpdate(const std::string& build, const boost::filesystem::path& path)
{
	if (build.find("win32") != string::npos) {
		if (AutoUpdater::doSelfUpdate(build, path, boost::filesystem::path(QCoreApplication::applicationFilePath().toStdString())))
			this->action_Quit->trigger();
	} else if (build.find("-deb-") != string::npos) {
		if (AutoUpdater::doSelfUpdate(build, path, "")) {
			boost::filesystem::path newtarget = path.branch_path() / (build +".deb");

			QDesktopServices::openUrl(QUrl::fromLocalFile(QString::fromStdString(newtarget.native_file_string())));

			QMessageBox::StandardButton res = QMessageBox::information(this,
				QString("Auto Update"),
				QString::fromStdString(string()
					+ "Downloading and verifying new uftt package succeeded.\r\n"
					+ "\r\n"
					+ "The package should be automatically opened now.\r\n"
					+ "If not, install it manually (" + newtarget.native_file_string() + ")\r\n"
					+ "\r\n"
					+ "After installing the package, restart uftt to use the new version.\r\n"
				)
			);
		}
	}
}

void MainWindow::on_listTasks_itemDoubleClicked(QTreeWidgetItem* twi, int col)
{
	string text = twi->text(0).toStdString();
	if (text.substr(0,4) != "uftt") return;
	size_t pos = text.find_last_of("\\/");
	if (pos == string::npos) return;
	string name = text.substr(pos+1);

	boost::filesystem::path path = DownloadEdit->text().toStdString();
	path /= name;

	if (ext::filesystem::exists(path)) {
		string spath;
		if (boost::filesystem::is_directory(path))
			spath += path.native_directory_string();
		else
			spath += path.native_file_string();
		QDesktopServices::openUrl(QUrl::fromLocalFile(QString::fromStdString(spath)));
	}
}

void MainWindow::on_listShares_itemDoubleClicked(QTreeWidgetItem* twi, int)
{
	if (listShares->currentItem() == twi)
		on_buttonDownload_clicked();
	else
		std::cout << "Doubleclicked on non-current share?\n";
}
