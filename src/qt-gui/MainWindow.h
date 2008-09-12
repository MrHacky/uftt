#ifndef MAIN_H
#define MAIN_H

#include "ui_MainWindow.h"

#include <boost/asio.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

#include <map>

#include "../UFTTSettings.h"
#include "QMarshaller.h"
#include "../net-asio/asio_http_request.h"


class QTreeWidgetItem;
class QCloseEvent;

class SimpleBackend;

class MainWindow: public QMainWindow, public Ui::MainWindow
{
	Q_OBJECT

	private:
		UFTTSettings& settings;
		bool askonupdates;
		std::string auto_update_url;
		boost::filesystem::path auto_update_path;
		SimpleBackend* backend;
		QTreeWidgetItem* ctwi;
		bool ctwiu;
		QMarshaller marshaller;

		void addLocalShare(std::string url);
		void setUpdateInterval(int i);
		void doSelfUpdate(const std::string& build, const boost::filesystem::path& path);

	protected:
		virtual void closeEvent(QCloseEvent * event);

	private Q_SLOTS:
		void DragStart(QTreeWidgetItem*, int);
		void onDragEnterTriggered(QDragEnterEvent* evt);
		void onDragMoveTriggered(QDragMoveEvent* evt);
		void onDropTriggered(QDropEvent* evt);
	public:
		MainWindow(UFTTSettings& settings_);
		void SetBackend(SimpleBackend* be);
		~MainWindow();

	public Q_SLOTS:
		void on_buttonRefresh_clicked();
		void on_buttonDownload_clicked();

		void on_buttonAdd1_clicked();
		void on_buttonAdd2_clicked();
		void on_buttonAdd3_clicked();

		void on_buttonBrowse_clicked();
		void on_buttonManualQuery_clicked();
		void on_buttonManualPublish_clicked();

		void on_actionEnableGlobalPeerfinder_toggled(bool);
		void on_actionEnableAutoupdate_toggled(bool);
		void on_actionCheckForWebUpdates_triggered();

		void on_buttonClearCompletedTasks_clicked();
		void on_listBroadcastHosts_itemChanged( QTreeWidgetItem * item, int column);

		void on_actionUpdateNever_toggled(bool);
		void on_actionUpdateDaily_toggled(bool);
		void on_actionUpdateWeekly_toggled(bool);
		void on_actionUpdateMonthly_toggled(bool);

		void on_listTasks_itemDoubleClicked(QTreeWidgetItem*, int);

		void check_autoupdate_interval();

	public: // callbacks
		void addSimpleShare(std::string sharename);

		void new_autoupdate(std::string url, std::string build, bool fromweb);

		void download_done(std::string url);
		void download_progress(QTreeWidgetItem* twi, uint64 tfx, std::string sts, boost::posix_time::ptime starttime, uint32 queue);
		void cb_web_download_done(const boost::system::error_code& err, const std::string& build, boost::shared_ptr<boost::asio::http_request> req);

		void new_upload(std::string name, int num);

		void onshow();
};

#endif
