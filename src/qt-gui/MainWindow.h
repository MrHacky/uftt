#ifndef MAIN_H
#define MAIN_H

#include "ui_MainWindow.h"

#include <QSystemTrayIcon>

#include <boost/asio.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

#include <map>

#include "../UFTTSettings.h"
#include "QMarshaller.h"
//#include "../net-asio/asio_http_request.h"

#include "../UFTTCore.h"

class QTreeWidgetItem;
class QCloseEvent;

class MainWindow: public QMainWindow, public Ui::MainWindow
{
	Q_OBJECT

	private:
		UFTTSettingsRef settings;
		bool askonupdates;
		ShareID auto_update_share;
		boost::filesystem::path auto_update_path;
		std::string auto_update_build;
		UFTTCore* backend;
		QTreeWidgetItem* ctwi;
		bool ctwiu;
		QMarshaller marshaller;
		QSystemTrayIcon* trayicon;
		QMenu* traymenu;

		uint32 timerid;
		bool isreallyactive;

		void timerLostFocus(uint32 tid);

		void addLocalShare(std::string url);
		void setUpdateInterval(int i);
		void setTrayDoubleClick(bool b);
		void doSelfUpdate(const std::string& build, const boost::filesystem::path& path);

		void new_autoupdate(const ShareInfo& info);
		void download_done(const ShareID& sid);

	protected:
		// overridden events
		virtual void closeEvent(QCloseEvent * evnt);
		virtual void hideEvent(QHideEvent * evnt);

	private Q_SLOTS:
		void DragStart(QTreeWidgetItem*, int);
		void onDragEnterTriggered(QDragEnterEvent* evt);
		void onDragMoveTriggered(QDragMoveEvent* evt);
		void onDropTriggered(QDropEvent* evt);
		void onFocusChanged(QWidget* old, QWidget* now);

	public:
		MainWindow(UFTTSettingsRef settings_);
		void SetBackend(UFTTCore* be);
		~MainWindow();

	public Q_SLOTS:
		void do_refresh_shares();

		void on_buttonRefresh_clicked();
		void on_buttonDownload_clicked();

		void on_buttonAdd1_clicked();
		void on_buttonAdd2_clicked();
		void on_buttonAdd3_clicked();

		void on_buttonBrowse_clicked();
		void on_buttonManualQuery_clicked();
		void on_buttonManualPublish_clicked();

		void on_actionEnableGlobalPeerfinder_toggled(bool);
		void on_actionEnableDownloadResume_toggled(bool);
		void on_actionEnableAutoupdate_toggled(bool);
		void on_actionCheckForWebUpdates_triggered();

		void on_actionAboutUFTT_triggered();
		void on_actionAboutQt_triggered();

		void on_buttonClearCompletedTasks_clicked();
		void on_listBroadcastHosts_itemChanged(QTreeWidgetItem* item, int column);

		void on_actionUpdateNever_toggled(bool);
		void on_actionUpdateDaily_toggled(bool);
		void on_actionUpdateWeekly_toggled(bool);
		void on_actionUpdateMonthly_toggled(bool);

		void on_actionSingleClickToActivateTrayIcon_toggled(bool);
		void on_actionDoubleClickToActivateTrayIcon_toggled(bool);

		void on_listTasks_itemDoubleClicked(QTreeWidgetItem*, int);
		void on_listShares_itemActivated(QTreeWidgetItem*, int);

		void on_editNickName_textChanged(QString text);

		void handle_trayicon_activated(QSystemTrayIcon::ActivationReason);
	public: // callbacks
		void addSimpleShare(const ShareInfo& info);

		void new_task(const TaskInfo& info);

		void download_progress(QTreeWidgetItem* twi, boost::posix_time::ptime starttime, const TaskInfo& ti);

		void pre_show();
		void post_show();
};

#endif
