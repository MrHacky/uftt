#include "MainWindow.moc"

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
#include <QMetaType>
#include <QMimeData>
#include <QProcess>
#include <QStringList>
#include <QSystemTrayIcon>
#include <QTimer>
#include <QTreeWidgetItem>
#include <QUrl>
#include <QWidget>
#include <QScrollBar>
#include <QSysInfo>

#include <boost/foreach.hpp>

#include "QDebugStream.h"
#include "QToggleHeaderAction.h"

#include "../Platform.h"
#include "../AutoUpdate.h"
#include "../util/StrFormat.h"
#include "../util/Filesystem.h"
#include "../util/asio_timer_oneshot.h"

#include "../Globals.h"

#include "DialogDirectoryChooser.h"

#include "DialogPreferences.h"

// Register ShareID to allow using it inside a QVariant so it can be attached to a tree widget item
Q_DECLARE_METATYPE(ShareID);

using namespace std;

enum ShareListColumNames {
	SLCN_USER = 0,
	SLCN_SHARE,
	SLCN_HOST,
	SLCN_PROTOCOL,
	SLCN_URL,
};

enum TaskListColumNames {
	TLCN_SHARE = 0,
	TLCN_STATUS,
	TLCN_HOST,
	TLCN_USER,
	TLCN_TIME,
	TLCN_ETA,
	TLCN_TRANSFERRED,
	TLCN_SIZE,
	TLCN_SPEED,
	TLCN_QUEUE,
};


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

MainWindow::MainWindow(UFTTSettingsRef settings_)
: settings(settings_)
, isreallyactive(false)
, quitting(false)
, ishiding(false)
, timerid(0)
, dialogPreferences(NULL)
{
	setupUi(this);

	// workaround: setting a stylesheet directly on this QMainWindow messes up collumn widths in QTreeWidgets somehow
	this->setStyleSheet(centralwidget->styleSheet());

	string stylename = this->style()->metaObject()->className();
	if (stylename == "QWindowsXPStyle" || stylename == "QWindowsVistaStyle") {
		// these styles have invisible separators sometimes,
		// that looks horrible, so make them visible
		// note, not actually tested with all themes of these styles
		//       hope it doesn't make any of them really ugly or something...
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
		this->setStyleSheet(this->styleSheet() + "\n" + QString(sepstyle) + "\n");
	}

	// setup debug stream
	debugstream = new QDebugStream(std::cout, marshaller.wrap(boost::bind(&QTextEdit::append, debugText, _1)));

	// remove central widget so everything is dockable
	{
		QWidget* cw = new QWidget();
		this->setCentralWidget(cw);
		delete cw;
	}

	// fix statusbar so it will look less ugly
	{
		QStatusBar* bar = ((QMainWindow*)this)->statusBar();
		int left, top, right, bottom;
		bar->getContentsMargins(&left, &top, &right, &bottom);
		top += this->style()->pixelMetric(QStyle::PM_DockWidgetSeparatorExtent);
		if((stylename != "QCleanlooksStyle") && (stylename != "QGtkStyle")) {
			bar->setContentsMargins( left,  top,  right,  bottom);
		}
		//bar->addPermanentWidget(new QLabel(QString::fromStdString(thebuildstring)));
		QLabel* imagelabel = new QLabel();
		//QImage image1(":/status/status-ok.png");
		//imagelabel->setPixmap(QPixmap::fromImage(image1));
		//bar->addPermanentWidget(imagelabel);
		//bar->addPermanentWidget(new QLabel(QString::fromStdString(thebuildstring)));
		bar->addWidget(new QLabel("Status bar"));
	}

	this->setDockOptions(QMainWindow::DockOptions(QMainWindow::AllowNestedDocks|QMainWindow::AllowTabbedDocks|QMainWindow::AnimatedDocks|QMainWindow::VerticalTabs));

	BOOST_FOREACH(QObject* obj, children()) {
		if (QString(obj->metaObject()->className()) == "QDockWidget") {
			QDockWidget* dock = (QDockWidget*)obj;
			menu_View->addAction(dock->toggleViewAction());
			dock->setAllowedAreas(Qt::TopDockWidgetArea);
			dock->setFeatures(QDockWidget::DockWidgetClosable | QDockWidget::DockWidgetMovable/* | QDockWidget::DockWidgetFloatable*/);
			connect(dock, SIGNAL(dockLocationChanged(Qt::DockWidgetArea)), this, SLOT(update()));
			this->addDockWidget(Qt::TopDockWidgetArea, dock);
		}
	}

	buttonAdd2->hide();

	listBroadcastHosts->hide();
	label_3->hide();
	buttonResolveBroadcast->hide();


	/* load/set dock layout */
	if (ext::filesystem::exists("uftt.layout")) {
		QFile layoutfile("uftt.layout");
		if (layoutfile.open(QIODevice::ReadOnly)) {
			QRect rect;
			layoutfile.read((char*)&rect, sizeof(QRect));
			settings->posx = rect.x();
			settings->posy = rect.y();
			settings->sizex = rect.width();
			settings->sizey = rect.height();
			QByteArray data = layoutfile.readAll();
			settings->dockinfo = std::vector<uint8>((uint8*)data.data(), (uint8*)data.data()+data.size());
			layoutfile.remove();
		}
	}

	/* apply size settings */
	if (settings->sizex != 0 && settings->sizey !=0) {
		this->resize(QSize(settings->sizex, settings->sizey));
		this->move(QPoint(settings->posx, settings->posy));
		if (settings->guimaximized) this->setWindowState(this->windowState() | Qt::WindowMaximized);
	} else
		this->resize(750, 550);

	/* apply dock layout */
	{
		std::vector<uint8> dockinfo = settings->dockinfo;
		if (dockinfo.size() > 0)
			this->restoreState(QByteArray((char*)&dockinfo[0], dockinfo.size()));
		else {
			this->splitDockWidget (dockShares   , dockTaskList      , Qt::Horizontal);
			this->splitDockWidget (dockTaskList , dockWidgetDebug   , Qt::Vertical  );
			this->splitDockWidget (dockShares   , dockManualConnect , Qt::Vertical  );
			this->dockManualConnect->hide();
			#ifdef NDEBUG
				this->dockWidgetDebug->hide();
			#endif
		}
	}

	/* set sharelist layout */
	//this->listShares->header()->moveSection(2+0,0);
	//this->listShares->header()->moveSection(1+1,1);
	//this->listShares->header()->moveSection(0+2,2);
	//this->listShares->header()->moveSection(3+0,3);
	this->listShares->hideColumn(SLCN_URL);
	this->listShares->hideColumn(SLCN_PROTOCOL);
	this->listShares->setSortingEnabled(true);
	this->listTasks->setSortingEnabled(true);
	QToggleHeaderAction::addActions(this->listShares);
	QToggleHeaderAction::addActions(this->listTasks);

	if (!ext::filesystem::exists(settings->dl_path)) {
		boost::filesystem::path npath(platform::getDownloadPathDefault());
		if (ext::filesystem::exists(npath))
			settings->dl_path = npath;
	}

	this->editDownload->setText(QString::fromStdString(settings->dl_path.get().native_directory_string()));

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

	{
		QIcon* uftticon = new QIcon();
#ifdef Q_WS_WIN
		if (QSysInfo::WindowsVersion & (QSysInfo::WV_32s|QSysInfo::WV_95|QSysInfo::WV_98))
			uftticon->addFile(":/icon/uftt-16x16x4.png");
		else
#endif
			uftticon->addFile(":/icon/uftt-16x16.png");
		uftticon->addFile(":/icon/uftt-32x32.png");
		uftticon->addFile(":/icon/uftt-48x48.png");
		// TODO: add 22x22 icon as that apparently is preferred for trayicons on linux
		trayicon = new QSystemTrayIcon(*uftticon, this);

		connect(trayicon, SIGNAL(activated(QSystemTrayIcon::ActivationReason))  , this, SLOT(handle_trayicon_activated(QSystemTrayIcon::ActivationReason)));

		this->setWindowIcon(*uftticon);
	}

	traymenu = new QMenu("tray menu", this);
	traymenu->addAction(action_Quit);
}

void MainWindow::handle_trayicon_activated(QSystemTrayIcon::ActivationReason reason)
{
	switch (reason) {
		case QSystemTrayIcon::Trigger:
		case QSystemTrayIcon::DoubleClick: {
			if (settings->traydoubleclick && reason == QSystemTrayIcon::Trigger) return;
			if (!settings->traydoubleclick && reason == QSystemTrayIcon::DoubleClick) return;
			if (isreallyactive)
				hideToTray();
			else
				showFromTray();
		}; break;
		case QSystemTrayIcon::Context: {
			traymenu->exec(QCursor::pos());
		}; break;
		default: /* nothing */ ;
	}
}

void MainWindow::onFocusChanged(QWidget* old, QWidget* now)
{
	if (this->isActiveWindow()) {
		++timerid;
		isreallyactive = true;
	} else if (isreallyactive) {
		++timerid;
		asio_timer_oneshot(backend->get_io_service(), 500, marshaller.wrap(boost::bind(&MainWindow::timerLostFocus, this, timerid)));
	}
}

void MainWindow::timerLostFocus(uint32 tid)
{
	if (tid == timerid) {
		++timerid;
		isreallyactive = false;
	}
}

void MainWindow::quit()
{
	quitting = true;
	close();
}

void MainWindow::saveGeometry()
{
	if (!isMaximized() && !isMinimized()) {
		settings->posx = this->pos().x();
		settings->posy = this->pos().y();
		settings->sizex = this->size().width();
		settings->sizey = this->size().height();
	}
	settings->guimaximized = isMaximized();
}

void MainWindow::resizeEvent(QResizeEvent* evnt)
{
	saveGeometry();
}

void MainWindow::moveEvent(QMoveEvent* evnt)
{
	saveGeometry();
}

void MainWindow::closeEvent(QCloseEvent * evnt)
{
	if (!quitting && settings->close_to_tray && hideToTray()) {
		evnt->ignore();
		return;
	}

	/* put stuff back into settings */
	saveGeometry();
	QByteArray data = saveState();
	settings->dockinfo = std::vector<uint8>((uint8*)data.data(), (uint8*)data.data()+data.size());

	trayicon->hide();
}

void MainWindow::changeEvent(QEvent * evnt)
{
	if (evnt->type() != QEvent::WindowStateChange) return;
	if (!isMinimized()) return;

	// This was a hide event
	if (ishiding) return;
	if (settings->minimize_to_tray && hideToTray())
		return;

	++timerid;
	this->isreallyactive = false;
}

bool MainWindow::hideToTray()
{
	trayicon->show();
	if (!trayicon->isVisible()) return false;

	QTimer::singleShot(0, this, SLOT(hide()));

	++timerid;
	this->isreallyactive = false;

	return true;
}

bool MainWindow::showFromTray()
{
	//if (!settings->tray_show_always) trayicon->hide();

	if (!this->isActiveWindow()) {
		ishiding = true;
		this->hide();
		ishiding = false;
	}
	this->setVisible(true);
	if (this->isMinimized()) this->showNormal();
	this->activateWindow();

	this->raise();

	++timerid;
	this->isreallyactive = true;

	return true;
}

void MainWindow::handleGuiCommand(UFTTCore::GuiCommand cmd)
{
	if (cmd == UFTTCore::GUI_CMD_SHOW) showFromTray();
}

MainWindow::~MainWindow()
{
	delete debugstream;
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

void MainWindow::do_refresh_shares() {
	backend->doRefreshShares();
}

void MainWindow::on_buttonRefresh_clicked()
{
	if (!(qApp->keyboardModifiers() & Qt::ShiftModifier)) {
		listShares->clear();
	}
	if (!(qApp->keyboardModifiers() & Qt::ControlModifier)) {
		for(int i=0; i<8; ++i) {
			QTimer::singleShot(i*20, this, SLOT(do_refresh_shares()));
		}
	}
}

void MainWindow::addSimpleShare(const ShareInfo& info)
{
	//if (info.islocal) return;
	if (info.isupdate) {
		new_autoupdate(info);
		return;
	}

	QString quser  = QString::fromStdString(info.user);
	QString qshare = QString::fromStdString(info.name);
	QString qproto = QString::fromStdString(info.proto);
	QString qhost  = QString::fromStdString(info.host);
	QString qurl   = QString::fromStdString(STRFORMAT("%s:\\\\%s\\%s", info.proto, info.host, info.name));
	if(quser == "") {
		quser = "uftt-user";
	}
	uint32 version = atoi(info.proto.substr(6).c_str());

	QList<QTreeWidgetItem*> fres = listShares->findItems(qshare, Qt::MatchExactly, SLCN_SHARE);

	bool found = false;
	BOOST_FOREACH(QTreeWidgetItem* twi, fres) {
		if (twi->text(SLCN_HOST) == qhost && twi->text(SLCN_SHARE) == qshare) {
			found = true;
			QString qoprot = twi->text(SLCN_PROTOCOL);
			uint32 over = atoi(qoprot.toStdString().substr(6).c_str());
			if (over <= version) {
				twi->setText(SLCN_USER, quser);
				twi->setText(SLCN_PROTOCOL, qproto);
				twi->setText(SLCN_URL, qurl);
			}
		}
	}

	if (!found) {
		QTreeWidgetItem* rwi = new QTreeWidgetItem(listShares, 0);
		rwi->setText(SLCN_USER, quser);
		rwi->setText(SLCN_SHARE, qshare);
		rwi->setText(SLCN_HOST, qhost);
		rwi->setText(SLCN_PROTOCOL, qproto);
		rwi->setText(SLCN_URL, qurl);
		rwi->setData(0, Qt::UserRole, QVariant::fromValue(info.id));
	}
}

void MainWindow::DragStart(QTreeWidgetItem* rwi, int col)
{
	if (rwi != NULL && col == 0) {
		QDrag *drag = new QDrag(this);
		QMimeData *mimeData = new QMimeData;

		// TODO: figure this out...
		//mimeData->setText(rwi->text(0));
		//drag->setMimeData(mimeData);
		//drag->setPixmap(iconPixmap);

		// Qt::DropAction dropAction = drag->start();
	}
}

std::string MainWindow::getDownloadPath()
{
	std::string path = settings->dl_path.get().string();
	if (!path.empty()) {
		char c = path[path.size()-1];
		if (c != '\\' && c != '/')
			path.push_back('/');
	}
	return boost::filesystem::path(path).native_directory_string();
}

void MainWindow::on_editDownload_textChanged(QString text)
{
	boost::filesystem::path dl_path = text.toStdString();
	settings->dl_path = dl_path;
	editDownload->setProperty("isValid", ext::filesystem::exists(dl_path));
	editDownload->setStyleSheet(editDownload->styleSheet()); // recalculate style
}

void MainWindow::on_buttonDownload_clicked()
{
	boost::filesystem::path dlpath = getDownloadPath();
	if (!ext::filesystem::exists(dlpath)) {
		QMessageBox::information (this, "Download Failed", "Select a valid download directory first");
		return;
	}
	QList<QTreeWidgetItem*> selected = listShares->selectedItems();

	BOOST_FOREACH(QTreeWidgetItem* rwi, selected) {
		ShareID sid = rwi->data(0, Qt::UserRole).value<ShareID>();
		if (qApp->keyboardModifiers() & Qt::ShiftModifier) {
			// evil hax!!!
			sid.sid[0] = 'x';
		}
		backend->startDownload(sid, dlpath);
	}
}

void MainWindow::download_progress(QTreeWidgetItem* twi, boost::posix_time::ptime starttime, const TaskInfo& ti)
{
	boost::posix_time::ptime curtime = boost::posix_time::second_clock::universal_time();
	boost::posix_time::time_duration elapsed = curtime-starttime;

	uint64 tfx = ti.transferred;
	std::string sts = ti.status;
	uint32 queue = ti.queue;
	uint64 total = ti.size;

	std::string type = (ti.isupload ? "U: " : "D: ");
	twi->setText(TLCN_SHARE, QString::fromStdString(type + ti.shareinfo.name));
	twi->setText(TLCN_STATUS, QString::fromStdString(sts));
	twi->setText(TLCN_HOST, QString::fromStdString(ti.shareinfo.host));
	twi->setText(TLCN_USER, QString::fromStdString(ti.shareinfo.user));
	twi->setText(TLCN_TIME, QString::fromStdString(boost::posix_time::to_simple_string(elapsed)));
	if (total > 0 && total >= tfx && tfx > 0) {
		twi->setText(TLCN_ETA, QString::fromStdString(
			boost::posix_time::to_simple_string(
				boost::posix_time::time_duration(
					boost::posix_time::seconds(
						(total-tfx) * elapsed.total_seconds() / tfx
					)
				)
			)
		));
	}
	twi->setText(TLCN_TRANSFERRED, QString::fromStdString(StrFormat::bytes(tfx)));
	twi->setText(TLCN_SIZE, QString::fromStdString(StrFormat::bytes(total)));
	if (elapsed.total_seconds() > 0) {
		twi->setText(TLCN_SPEED, QString::fromStdString(STRFORMAT("%s\\s", StrFormat::bytes(tfx/elapsed.total_seconds()))));
	}
	twi->setText(TLCN_QUEUE, QString::fromStdString(STRFORMAT("%d", queue)));
	if (!ti.isupload && sts == "Completed")
		download_done(ti.shareid);
}

void MainWindow::addLocalShare(std::string url)
{
	boost::filesystem::path path = url;
	if (path.leaf() == ".") path.remove_leaf(); // linux thingy
	backend->addLocalShare(path.leaf(), path);
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
		tr("Choose download directory"),
		QString::fromStdString(getDownloadPath()));
	if (!directory.isEmpty())
		this->editDownload->setText(QString::fromStdString(boost::filesystem::path(directory.toStdString()).native_directory_string()));
}

void MainWindow::new_autoupdate(const ShareInfo& info)
{
	std::string build = info.name;

	bool fromweb = (info.proto == "http");
	if (!fromweb && !settings->autoupdate)
		return;

	if (!AutoUpdater::isBuildBetter(build, thebuildstring)) {
		cout << "ignoring update: " << build << " @ " << info.host << '\n';
		return;
	}
	cout << "new autoupdate: " << build << " @ " << info.host << '\n';
	static bool dialogshowing = false;
	if (dialogshowing) return;
	dialogshowing = true;
	QMessageBox::StandardButton res = QMessageBox::question(this,
		QString("Auto Update"),
		QString::fromStdString(
			"Build: " + build + '\r' + '\n' +
			"Host: " + info.host + '\r' + '\n'
		),
		QMessageBox::Yes|QMessageBox::No|QMessageBox::NoToAll,
		QMessageBox::No);
	dialogshowing = false;

	if (res == QMessageBox::NoToAll) {
		if (!fromweb)
			settings->autoupdate = false;
		else
			settings->webupdateinterval = 0; // never
	}
	if (res != QMessageBox::Yes)
		return;

	auto_update_share = info.id;
	auto_update_path = getDownloadPath();
	auto_update_build = build;

	backend->startDownload(auto_update_share, auto_update_path);
}

void MainWindow::download_done(const ShareID& sid)
{
	//cout << "download complete: " << sid.name << " (" << sid.host << ")\n";
	if (sid == auto_update_share) {
		std::string bname = auto_update_build;
		string fname = bname;
		if (bname.find("win32") != string::npos) fname += ".exe";
		if (bname.find("-deb-") != string::npos) fname += ".deb.signed";
		auto_update_path /= fname;
		cout << "autoupdate: " << auto_update_path << "!\n";

		this->doSelfUpdate(bname, auto_update_path);
	}
}

void MainWindow::SetBackend(UFTTCore* be)
{
	backend = be;

	backend->connectSigAddShare(
		marshaller.wrap(boost::bind(&MainWindow::addSimpleShare, this, _1))
	);
	backend->connectSigNewTask(
		marshaller.wrap(boost::bind(&MainWindow::new_task, this, _1))
	);
	backend->connectSigGuiCommand(
		marshaller.wrap(boost::bind(&MainWindow::handleGuiCommand, this, _1))
	);

	backend->setMainWindowId(boost::lexical_cast<std::string>(this->winId()));

	if (backend->error_state == 2) {
		QMessageBox::critical( 0, "UFTT", QString::fromStdString(backend->error_string) + "\n\nApplication will now exit." );
		throw int(1); // thrown integers will quit application with integer as exit code
		//throw std::runtime_error(std::string("Fatal Error: ") + backend->error_string);
	}

	if (backend->error_state == 1) {
		QMessageBox::warning( 0, "UFTT", QString::fromStdString(backend->error_string));
	}
}

void MainWindow::on_buttonManualQuery_clicked()
{
	try {
		backend->doManualQuery(editManualQuery->text().toStdString());
	} catch (std::exception& e) {
		cout << "exception: " << e.what() << '\n';
	}
}

void MainWindow::on_buttonManualPublish_clicked()
{
	try {
		backend->doManualPublish(editManualPublish->text().toStdString());
	} catch (std::exception& e) {
		cout << "exception: " << e.what() << '\n';
	}
}

void MainWindow::on_actionAboutUFTT_triggered()
{
	QMessageBox::about( this, "About UFTT", QString() +
		"<h3>UFTT - Ultimate File Transfer Tool</h3>" +
		"<p>A simple no-nonsense tool for transferring files</p>" +
		"<p>Build number: " + QString::fromStdString(thebuildstring) + "</p>" +
		//"Copyright 1991-2003 Such-and-such. "
		//"Licenced under GPL\n\n"
		"<p>See <a href=\"http://code.google.com/p/uftt/\">http://code.google.com/p/uftt/</a> for more information.</p>" );
}

void MainWindow::on_actionPreferences_triggered()
{
	if (!dialogPreferences) dialogPreferences = new DialogPreferences(this, settings.get());

	dialogPreferences->exec();
}

void MainWindow::on_actionAboutQt_triggered()
{
	QMessageBox::aboutQt(this);
}

void MainWindow::on_buttonClearCompletedTasks_clicked()
{
	for(int i = this->listTasks->topLevelItemCount(); i > 0; --i) {
		QTreeWidgetItem* twi = this->listTasks->topLevelItem(i-1);
		if (twi->text(TLCN_STATUS) == "Completed")
			delete twi;
	}
}

void MainWindow::on_actionCheckForWebUpdates_triggered()
{
	backend->doManualQuery("webupdate");
}

#ifdef __linux__
void linuxQTextEditScrollFix(QTextEdit* qedit) { // Work around bug in Qt (linux only?)
	QString tmp = qedit->toPlainText();
	qedit->clear();
	qedit->append(tmp);
}
#endif

void MainWindow::pre_show() {
	//trayicon->show();
}

void MainWindow::post_show() {
#ifdef __linux__
	boost::thread thread(
		marshaller.wrap(boost::bind(&linuxQTextEditScrollFix, debugText))
	);
#endif
	if (settings->start_in_tray)
		hideToTray();
	else //if (settings->tray_show_always)
		trayicon->show();
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
	string text = twi->text(TLCN_SHARE).toStdString();
	if (text.substr(0,3) != "D: ") return;
	string name = text.substr(3);

	boost::filesystem::path path = getDownloadPath();
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

void MainWindow::on_listShares_itemActivated(QTreeWidgetItem*, int)
{
	on_buttonDownload_clicked();
}

void MainWindow::new_task(const TaskInfo& info)
{
	QTreeWidgetItem* twi = new QTreeWidgetItem(listTasks);
	twi->setText(TLCN_SHARE, QString::fromStdString(info.shareinfo.name));

	boost::posix_time::ptime starttime = boost::posix_time::second_clock::universal_time();
	boost::function<void(const TaskInfo&)> handler =
		marshaller.wrap(boost::bind(&MainWindow::download_progress, this, twi, starttime, _1));

	backend->connectSigTaskStatus(info.id, handler);
}
