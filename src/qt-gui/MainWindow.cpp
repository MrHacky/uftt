#include "MainWindow.moc"
#include <boost/asio.hpp>

#include <iostream>
#include <fstream>

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

#include "QDebugStream.h"

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

void tester(uint64 tfx, std::string sts)
{
	cout << "update: " << sts << ": " << tfx << '\n';
}

MainWindow::MainWindow(QTMain& mainimpl_)
: mainimpl(mainimpl_)
{
	qRegisterMetaType<std::string>("std::string");
	qRegisterMetaType<QTreeWidgetItem*>("QTreeWidgetItem*");
	qRegisterMetaType<uint64>("uint64");
	qRegisterMetaType<uint64>("uint32");
	qRegisterMetaType<boost::posix_time::ptime>("boost::posix_time::ptime");

	boost::filesystem::path settings_path;
	{
		spathlist spl = platform::getSettingPathList();
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

void MainWindow::on_buttonRefresh_clicked()
{
	backend->slot_refresh_shares();
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
	if (!boost::filesystem::exists(dlpath)) {
		QMessageBox::information (this, "Download Failed", "Select a valid download directory first");
		return;
	}
	QTreeWidgetItem* rwi = SharesTree->currentItem();
	if (!rwi)
		return;
	QString name = rwi->text(0);

	QTreeWidgetItem* twi = new QTreeWidgetItem(listTasks);
	twi->setText(0, name);

	boost::posix_time::ptime starttime = boost::posix_time::second_clock::universal_time();
	boost::function<void(uint64, std::string, uint32)> handler =
		marshaller.wrap(boost::bind(&MainWindow::download_progress, this, twi, _1, _2, starttime, _3));
	handler(0, "Starting", 0);

	backend->slot_download_share(name.toStdString(), dlpath, handler);
}


void MainWindow::download_progress(QTreeWidgetItem* twi, uint64 tfx, std::string sts, boost::posix_time::ptime starttime, uint32 queue)
{
	boost::posix_time::ptime curtime = boost::posix_time::second_clock::universal_time();
	boost::posix_time::time_duration elapsed = curtime-starttime;

	twi->setText(1, QString::fromStdString(StrFormat::bytes(tfx)));
	twi->setText(2, QString::fromStdString(boost::posix_time::to_simple_string(elapsed)));
	twi->setText(3, QString::fromStdString(sts));
	twi->setText(4, QString::fromStdString(STRFORMAT("%d", queue)));
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
	cout << "new autoupdate: " << build << " : " << url << '\n';
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
		boost::function<void(uint64, std::string, uint32)> handler = boost::bind(&tester, _1, _2);
		backend->slot_download_share(url, auto_update_path, handler);
	} else {
		backend->do_download_web_update(url,
			marshaller.wrap(boost::bind(&MainWindow::cb_web_download_done, this, _1, build, _2))
		);
	}
}

void MainWindow::download_done(std::string url)
{
	cout << "download complete: " << url << '\n';
	if (url == auto_update_url) {
		size_t pos = auto_update_url.find_last_of("\\/");
		string bname = auto_update_url.substr(pos+1);
		string fname = bname + ".exe";
		auto_update_path /= fname;
		cout << "autoupdate: " << auto_update_path << "!\n";

		if (AutoUpdater::doSelfUpdate(bname, auto_update_path, boost::filesystem::path(QCoreApplication::applicationFilePath().toStdString())))
			this->action_Quit->trigger();
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
	backend->sig_download_ready.connect(
		marshaller.wrap(boost::bind(&MainWindow::download_done, this, _1))
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
	boost::function<void(uint64, std::string, uint32)> handler =
		marshaller.wrap(boost::bind(&MainWindow::download_progress, this, twi, _1, _2, starttime, _3));

	handler(0, "Starting", 0);
	backend->slot_attach_progress_handler(num, handler);
}

void MainWindow::on_actionCheckForWebUpdates_triggered()
{
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

		if (AutoUpdater::doSelfUpdate(build, temp_path, boost::filesystem::path(QCoreApplication::applicationFilePath().toStdString())))
			this->action_Quit->trigger();
	}
}
