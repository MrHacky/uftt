#ifndef MAIN_H
#define MAIN_H

#include "QTMain.h"
#include "ui_MainWindow.h"
#include "UFTTSettings.h"

#include <map>

#include <boost/asio.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

class QTreeWidgetItem;
class QCloseEvent;

class SimpleBackend;

class MainWindow: public QMainWindow, public Ui::MainWindow
{
	Q_OBJECT

	private:
		UFTTSettings settings;
		QTMain& mainimpl;
		bool askonupdates;
		std::string auto_update_url;
		boost::filesystem::path auto_update_path;
		SimpleBackend* backend;
		QTreeWidgetItem* ctwi;
		bool ctwiu;

	protected:
		virtual void closeEvent(QCloseEvent * event);

	private Q_SLOTS:
		void DragStart(QTreeWidgetItem*, int);
		void StartDownload();
		void onDragEnterTriggered(QDragEnterEvent* evt);
		void onDragMoveTriggered(QDragMoveEvent* evt);
		void onDropTriggered(QDropEvent* evt);
	public:
		MainWindow(QTMain& mainimpl_);
		void SetBackend(SimpleBackend* be);
		~MainWindow();

	public Q_SLOTS:
		void RefreshButtonClicked();
		void addSimpleShare(std::string sharename);
		void addLocalShare(std::string url);

		void on_buttonAdd1_clicked();
		void on_buttonAdd2_clicked();
		void on_buttonAdd3_clicked();

		void on_buttonBrowse_clicked();
		void on_buttonManualQuery_clicked();
		void on_buttonManualPublish_clicked();

		void on_actionEnableAutoupdate_toggled(bool);

		void new_autoupdate(std::string url);
		void download_done(std::string url);

		void new_upload(std::string name, int num);

		void download_progress(QTreeWidgetItem* twi, uint64 tfx, std::string sts, boost::posix_time::ptime starttime, uint32 queue);
		void on_buttonClearCompletedTasks_clicked();

		void on_listBroadcastHosts_itemChanged( QTreeWidgetItem * item, int column);
};

#endif
