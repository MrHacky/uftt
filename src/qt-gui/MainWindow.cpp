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

#include "../AutoUpdate.h"
#include "../util/StrFormat.h"
#include "../util/Filesystem.h"
#include "../util/asio_timer_oneshot.h"

#include "../BuildString.h"

#include "QExtUTF8.h"

#include "DialogPreferences.h"

// Register types to allow using them inside a QVariant so they can be attached to a tree widget item
Q_DECLARE_METATYPE(ShareID);
Q_DECLARE_METATYPE(ext::filesystem::path);

using namespace std;

enum ShareListColumNames {
	SLCN_USER = 0,
	SLCN_SHARE,
	SLCN_HOST,
	SLCN_PROTOCOL,
	SLCN_URL,

	SLDATA_SHAREID = 0,
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

	TLDATA_PATH = 0,
};

class MyItemDelegate : public QItemDelegate
{
public:
	MyItemDelegate(QObject* parent = 0) : QItemDelegate(parent) {}

	QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem& option, const QModelIndex& index) const
	{
		// allow only first column to be edited
		if (index.column() == 0)
			return QItemDelegate::createEditor(parent, option, index);
		return 0;
	}
};

class QActionSeparator {
	public:
		static QAction* create()
		{
			QAction* a = new QAction(NULL);
			a->setSeparator(true);
			return a;
		}
};

MainWindow::MainWindow(UFTTSettingsRef settings_)
: settings(settings_)
, isreallyactive(false)
, isreallyactiveaction(false)
, quitting(false)
, ishiding(false)
, timerid(0)
, dialogPreferences(NULL)
, taskbarProgress(boost::lexical_cast<std::string>(this->winId()))
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
			bar->setContentsMargins(left, top, right, bottom);
		}
		//bar->addPermanentWidget(new QLabel(S2Q(thebuildstring)));
		QLabel* imagelabel = new QLabel();
		//QImage image1(":/status/status-ok.png");
		//imagelabel->setPixmap(QPixmap::fromImage(image1));
		//bar->addPermanentWidget(imagelabel);
		//bar->addPermanentWidget(new QLabel(S2Q(thebuildstring)));
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

	if (!ext::filesystem::exists(settings->dl_path.get())) {
		ext::filesystem::path npath(platform::getDownloadPathDefault());
		if (ext::filesystem::exists(npath))
			settings->dl_path = npath;
	}

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
		comboDownload->setRecentPaths(settings->recentdownloadpaths);
		comboDownload->addRecentPath(qext::path::toQStringDirectory(settings->dl_path.get()));
		comboDownload->addPath(qext::path::toQStringDirectory(platform::getDownloadPathDefault()));
	}
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

	{
		QMenu* traymenu = new QMenu("UFTT Tray Icon Menu", this);
		traymenu->addAction(actionShowHide);
		traymenu->addSeparator();
		traymenu->addAction(actionShareFile);
		traymenu->addAction(actionShareFolder);
		traymenu->addSeparator();
		traymenu->addAction(actionQuit);
		//traymenu->setDefaultAction(actionShowHide);
		trayicon->setContextMenu(traymenu);
	}
	{
		listShares->addAction(actionDownload);
		listShares->addAction(actionDownloadTo);
		listShares->addAction(QActionSeparator::create());
		listShares->addAction(actionShareFile);
		listShares->addAction(actionShareFolder);
		listShares->addAction(QActionSeparator::create());
		listShares->addAction(actionUnshare);
		listShares->addAction(QActionSeparator::create());
		listShares->addAction(actionRefresh);
	}
	{
		listTasks->addAction(actionTaskOpen);
		listTasks->addAction(actionTaskOpenContainingFolder);
		listTasks->addAction(QActionSeparator::create());
		listTasks->addAction(actionClearCompletedTasks);
	}
}

void MainWindow::handle_trayicon_activated(QSystemTrayIcon::ActivationReason reason)
{
	switch (reason) {
		case QSystemTrayIcon::Trigger:
		case QSystemTrayIcon::DoubleClick: {
			if (settings->traydoubleclick && reason == QSystemTrayIcon::Trigger) return;
			if (!settings->traydoubleclick && reason == QSystemTrayIcon::DoubleClick) return;
			isreallyactiveaction = isreallyactive;
			actionShowHide->trigger();
		}; break;
		case QSystemTrayIcon::Context: {
			isreallyactiveaction = isreallyactive;
		}; break;
		default: /* nothing */ ;
	}
}

void MainWindow::on_actionShowHide_triggered()
{
	if (isreallyactiveaction)
		hideToTray();
	else
		showFromTray();
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
	settings->recentdownloadpaths = comboDownload->recentPathsStd();

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

void MainWindow::hideEvent(QHideEvent* evnt)
{
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

#ifndef Q_WS_WIN
	// helps when you have multiple desktops
	if (!this->isActiveWindow()) {
		ishiding = true;
		this->hide();
		ishiding = false;
	}
#endif

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

void MainWindow::on_listBroadcastHosts_itemChanged(QTreeWidgetItem * item, int column)
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

void MainWindow::on_actionRefresh_triggered()
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
		if (!settings->showautoupdates) return;
	}

	QString quser  = qext::utf8::toQString(info.user);
	QString qshare = qext::utf8::toQString(info.name);
	QString qproto = S2Q(info.proto);
	QString qhost  = S2Q(info.host);
	QString qurl   = qext::utf8::toQString(STRFORMAT("%s:\\\\%s\\%s", info.proto, info.host, info.name));
	if (quser == "") quser = (info.isupdate ? "<Update>" : "uftt-user");
	uint32 version = info.proto.size() > 6 ? atoi(info.proto.substr(6).c_str()) : 0;

	QList<QTreeWidgetItem*> fres = listShares->findItems(qshare, Qt::MatchExactly, SLCN_SHARE);

	bool found = false;
	BOOST_FOREACH(QTreeWidgetItem* twi, fres) {
		if (twi->text(SLCN_HOST) == qhost && twi->text(SLCN_SHARE) == qshare) {
			found = true;
			QString qoprot = twi->text(SLCN_PROTOCOL);
			uint32 over = qoprot.size() > 6 ? atoi(Q2S(qoprot).substr(6).c_str()) : 0;
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
		rwi->setData(SLDATA_SHAREID, Qt::UserRole, QVariant::fromValue(info.id));
	}
}

ext::filesystem::path MainWindow::getDownloadPath()
{
	std::string path = settings->dl_path.get().string();
	if (!path.empty()) {
		char c = path[path.size()-1];
		if (c != '\\' && c != '/')
			path.push_back('/');
	}
	return ext::filesystem::path(path);
}

QString MainWindow::getDownloadPathQ()
{
	return qext::path::toQStringDirectory(getDownloadPath());
}

void MainWindow::on_comboDownload_currentPathChanged(QString text)
{
	ext::filesystem::path dl_path = qext::path::fromQString(text);
	settings->dl_path = dl_path;
}

void MainWindow::on_listShares_itemSelectionChanged()
{
	bool enable = false;
	bool local = false;

	BOOST_FOREACH(QTreeWidgetItem* rwi, listShares->selectedItems()) {
		enable = true;
		if (!local) local = backend->isLocalShare(qext::utf8::fromQString(rwi->text(SLCN_SHARE)));;
	}

	actionDownload->setEnabled(enable);
	actionDownloadTo->setEnabled(enable);
	actionUnshare->setEnabled(local);

	buttonDownload->setEnabled(enable);
}

void MainWindow::on_actionDownload_triggered()
{
	ext::filesystem::path dlpath = getDownloadPath();
	if (!ext::filesystem::exists(dlpath)) {
		QMessageBox::information(this, "Download Failed", "Select a valid download directory first");
		return;
	}
	comboDownload->addRecentPath(comboDownload->currentPath());

	QList<QTreeWidgetItem*> selected = listShares->selectedItems();
	BOOST_FOREACH(QTreeWidgetItem* rwi, selected) {
		ShareID sid = rwi->data(SLDATA_SHAREID, Qt::UserRole).value<ShareID>();
		if (qApp->keyboardModifiers() & Qt::ShiftModifier) {
			// evil hax!!!
			sid.sid[0] = 'x';
		}
		backend->startDownload(sid, dlpath);
	}
}

void MainWindow::on_actionDownloadTo_triggered()
{
	QString directory;
	directory = QFileDialog::getExistingDirectory(this,
		tr("Choose download directory"),
		getDownloadPathQ());

	ext::filesystem::path dlpath = qext::path::fromQString(directory);
	if (!ext::filesystem::exists(dlpath)) {
		QMessageBox::information(this, "Download Failed", "Select a valid download directory");
		return;
	}
	comboDownload->addPath(comboDownload->currentPath());

	QList<QTreeWidgetItem*> selected = listShares->selectedItems();
	BOOST_FOREACH(QTreeWidgetItem* rwi, selected) {
		ShareID sid = rwi->data(SLDATA_SHAREID, Qt::UserRole).value<ShareID>();
		if (qApp->keyboardModifiers() & Qt::ShiftModifier) {
			// evil hax!!!
			sid.sid[0] = 'x';
		}
		backend->startDownload(sid, dlpath);
	}
}

void MainWindow::on_actionUnshare_triggered()
{
	BOOST_FOREACH(QTreeWidgetItem* rwi, listShares->selectedItems()) {
		LocalShareID id;
		if (backend->getLocalShareID(qext::utf8::fromQString(rwi->text(SLCN_SHARE)), &id))
			backend->delLocalShare(id);
	}

	// FIXME: Should not be neccessary, we should get a notification
	//        from the core/backend
	actionRefresh->trigger();
}

void MainWindow::download_progress(QTreeWidgetItem* twi, boost::posix_time::ptime starttime, const TaskInfo& ti)
{
	boost::posix_time::ptime curtime = boost::posix_time::second_clock::universal_time();
	boost::posix_time::time_duration elapsed = curtime-starttime;

	uint64 tfx = ti.transferred;
	std::string sts = ti.getTaskStatusString();
	uint32 queue = ti.queue;
	uint64 total = ti.size;

	if (ti.status == TASK_STATUS_ERROR) {
		taskbarProgress.setStateError();
		if (!ti.isupload || false /* show upload failures */) {
			trayicon->showMessage(
				QString() + (ti.isupload ? "Upload" : "Download") + " Failed",
				QString() + "An error occured during transfer of '" +
					qext::utf8::toQString(ti.shareinfo.name) + "':\n" +
					S2Q(sts),
					QSystemTrayIcon::Critical
			);
		}
	}

	taskbarProgress.setValue(ti.transferred, ti.size);

	std::string type = (ti.isupload ? "U: " : "D: ");
	twi->setText(TLCN_SHARE, qext::utf8::toQString(type + ti.shareinfo.name));
	twi->setText(TLCN_STATUS, S2Q(sts));
	twi->setText(TLCN_HOST, S2Q(ti.shareinfo.host));
	twi->setText(TLCN_USER, S2Q(ti.shareinfo.user));
	twi->setText(TLCN_TIME, S2Q(boost::posix_time::to_simple_string(elapsed)));
	twi->setData(TLDATA_PATH, Qt::UserRole, QVariant::fromValue(ti.path));

	if (total > 0 && total >= tfx && tfx > 0) {
		twi->setText(TLCN_ETA, S2Q(
			boost::posix_time::to_simple_string(
				boost::posix_time::time_duration(
					boost::posix_time::seconds(
						(total-tfx) * elapsed.total_seconds() / tfx
					)
				)
			)
		));
	}
	twi->setText(TLCN_TRANSFERRED, S2Q(StrFormat::bytes(tfx)));
	twi->setText(TLCN_SIZE, S2Q(StrFormat::bytes(total)));
	if (elapsed.total_seconds() > 0) {
		twi->setText(TLCN_SPEED, S2Q(STRFORMAT("%s\\s", StrFormat::bytes(tfx/elapsed.total_seconds()))));
	}
	twi->setText(TLCN_QUEUE, S2Q(STRFORMAT("%d", queue)));
	if (!ti.isupload && (ti.status == TASK_STATUS_COMPLETED || ti.status == TASK_STATUS_ERROR))
		download_done(ti);
}

void MainWindow::addLocalShare(QString url)
{
	if (url.isEmpty()) return;
	std::string path = qext::utf8::fromQString(url);
	backend->addLocalShare(path);
}

void MainWindow::onDropTriggered(QDropEvent* evt)
{
	//LOG("try=" << Q2S(evt->mimeData()->text());
	evt->acceptProposedAction();

	BOOST_FOREACH(const QUrl & url, evt->mimeData()->urls())
		this->addLocalShare(url.toLocalFile());
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

void MainWindow::on_actionShareFolder_triggered()
{
	QString directory;
	directory = QFileDialog::getExistingDirectory(this,
		tr("Choose directory to share"),
		"");
	this->addLocalShare(directory);
}

void MainWindow::on_actionShareFile_triggered()
{
	QString directory;
	directory = QFileDialog::getOpenFileName (this, tr("Choose file to share"),
		"",
		tr("any (*)"));
	this->addLocalShare(directory);
}

void MainWindow::on_buttonBrowse_clicked()
{
	QString directory;
	directory = QFileDialog::getExistingDirectory(this,
		tr("Choose download directory"),
		getDownloadPathQ());
	if (!directory.isEmpty())
		this->comboDownload->addRecentPath(directory);
}

void MainWindow::new_autoupdate(const ShareInfo& info)
{
	std::string build = info.name;

	// stop prompting for updates when one is already downloading
	if (auto_update_build != "")
		return;

	bool fromweb = (info.proto == "http");
	if (!fromweb && !settings->autoupdate)
		return;

	if (!AutoUpdater::isBuildBetter(build, get_build_string(), settings->minbuildtype)) {
		cout << "ignoring update: " << build << " @ " << info.host << '\n';
		return;
	}
	cout << "new autoupdate: " << build << " @ " << info.host << '\n';
	static bool dialogshowing = false;
	if (dialogshowing) return;
	dialogshowing = true;
	QMessageBox::StandardButton res = QMessageBox::question(this,
		QString("Auto Update"),
		S2Q(
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
	auto_update_path /= build;
}

void MainWindow::download_done(const TaskInfo& ti)
{
	if (ti.status == TASK_STATUS_COMPLETED)
		trayicon->showMessage("Download Completed", qext::utf8::toQString(ti.shareinfo.name));

	if (ti.shareid == auto_update_share) {
		if (ti.status == TASK_STATUS_COMPLETED) {
			cout << "autoupdate: " << auto_update_path << "!\n";
			this->doSelfUpdate(auto_update_build, auto_update_path);
		}
		auto_update_build = "";
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
		QMessageBox::critical(0, "UFTT", S2Q(backend->error_string) + "\n\nApplication will now exit." );
		throw int(1); // thrown integers will quit application with integer as exit code
		//throw std::runtime_error(std::string("Fatal Error: ") + backend->error_string);
	}

	if (backend->error_state == 1) {
		QMessageBox::warning(0, "UFTT", S2Q(backend->error_string));
	}
}

void MainWindow::on_buttonManualQuery_clicked()
{
	try {
		backend->doManualQuery(Q2S(editManualQuery->text()));
	} catch (std::exception& e) {
		cout << "exception: " << e.what() << '\n';
	}
}

void MainWindow::on_buttonManualPublish_clicked()
{
	try {
		backend->doManualPublish(Q2S(editManualPublish->text()));
	} catch (std::exception& e) {
		cout << "exception: " << e.what() << '\n';
	}
}

void MainWindow::on_actionAboutUFTT_triggered()
{
	QMessageBox::about(this, "About UFTT", QString() +
		"<h3>UFTT - Ultimate File Transfer Tool</h3>" +
		"<p>A simple no-nonsense tool for transferring files</p>" +
		"<p>Build number: " + S2Q(get_build_string()) + "</p>" +
		//"Copyright 1991-2003 Such-and-such. "
		//"Licenced under GPL\n\n"
		"<p>See <a href=\"http://code.google.com/p/uftt/\">http://code.google.com/p/uftt/</a> for more information.</p>" );
}

void MainWindow::on_actionPreferences_triggered()
{
	if (!dialogPreferences) dialogPreferences = new DialogPreferences(settings.get(), this);

	dialogPreferences->exec();
}

void MainWindow::on_actionAboutQt_triggered()
{
	QMessageBox::aboutQt(this);
}

void MainWindow::on_actionTaskOpen_triggered()
{
	QTreeWidgetItem* twi = listTasks->currentItem();
	ext::filesystem::path path = twi->data(TLDATA_PATH, Qt::UserRole).value<ext::filesystem::path>();
	string sharename = qext::utf8::fromQString(twi->text(TLCN_SHARE).mid(3));
	path /= sharename;

	if (ext::filesystem::exists(path))
		if (boost::filesystem::is_directory(path))
			QDesktopServices::openUrl(QUrl::fromLocalFile(qext::path::toQStringDirectory(path)));
		else
			QDesktopServices::openUrl(QUrl::fromLocalFile(qext::path::toQStringFile(path)));
}

void MainWindow::on_actionTaskOpenContainingFolder_triggered()
{
	QTreeWidgetItem* twi = listTasks->currentItem();
	ext::filesystem::path dirpath = twi->data(TLDATA_PATH, Qt::UserRole).value<ext::filesystem::path>();
	string sharename = qext::utf8::fromQString(twi->text(TLCN_SHARE).mid(3));
	ext::filesystem::path shrpath = dirpath / sharename;

	if (ext::filesystem::exists(shrpath) && platform::openContainingFolder(shrpath)) {
		// openContainingFolder succeeded!
	} else if (ext::filesystem::exists(dirpath))
		QDesktopServices::openUrl(QUrl::fromLocalFile(qext::path::toQStringDirectory(dirpath)));
}

void MainWindow::on_actionClearCompletedTasks_triggered()
{
	for(int i = this->listTasks->topLevelItemCount(); i > 0; --i) {
		QTreeWidgetItem* twi = this->listTasks->topLevelItem(i-1);
		if (twi->text(TLCN_STATUS) == "Completed")
			delete twi;
	}
}

void MainWindow::on_listTasks_itemSelectionChanged()
{
	QTreeWidgetItem* twi = listTasks->currentItem();
	if (twi) {
		string sharename = qext::utf8::fromQString(twi->text(TLCN_SHARE));
		bool isupload = sharename.size() > 3 && (sharename.substr(0,3) == "U: ");
		bool completed = (twi->text(TLCN_STATUS) == "Completed");

		actionTaskOpenContainingFolder->setEnabled(true);
		actionTaskOpen->setEnabled(isupload || completed);
	} else {
		actionTaskOpenContainingFolder->setEnabled(false);
		actionTaskOpen->setEnabled(false);
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

void MainWindow::doSelfUpdate(const std::string& build, const ext::filesystem::path& path)
{
	if (build.find("win32") != string::npos) {
		if (AutoUpdater::doSelfUpdate(build, path, qext::path::fromQString(QCoreApplication::applicationFilePath())))
			this->actionQuit->trigger();
	} else if (build.find("-deb-") != string::npos) {
		if (AutoUpdater::doSelfUpdate(build, path, "")) {
			ext::filesystem::path newtarget = path.parent_path() / (build +".deb");

			QDesktopServices::openUrl(QUrl::fromLocalFile(qext::path::toQStringFile(newtarget)));

			QMessageBox::StandardButton res = QMessageBox::information(this,
				QString("Auto Update"),
				QString()
					+ "Downloading and verifying new uftt package succeeded.\r\n"
					+ "\r\n"
					+ "The package should be automatically opened now.\r\n"
					+ "If not, install it manually (" + qext::path::toQStringFile(newtarget) + ")\r\n"
					+ "\r\n"
					+ "After installing the package, restart uftt to use the new version.\r\n"
			);
		}
	}
}

void MainWindow::new_task(const TaskInfo& info)
{
	QTreeWidgetItem* twi = new QTreeWidgetItem(listTasks);
	twi->setText(TLCN_SHARE, qext::utf8::toQString(info.shareinfo.name));

	boost::posix_time::ptime starttime = boost::posix_time::second_clock::universal_time();
	boost::function<void(const TaskInfo&)> handler =
		marshaller.wrap(boost::bind(&MainWindow::download_progress, this, twi, starttime, _1));

	backend->connectSigTaskStatus(info.id, handler);
}
