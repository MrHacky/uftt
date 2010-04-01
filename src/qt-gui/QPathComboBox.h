#ifndef Q_PATH_COMBOBOX_H
#define Q_PATH_COMBOBOX_H

#include <QComboBox>

class QPathComboBox: public QComboBox {
	Q_OBJECT

	public:
		QPathComboBox(QWidget* parent = NULL);

		void setRecentPaths(QStringList paths);
		QStringList recentPaths(size_t max = 10);

		void setRecentPaths(const std::vector<std::string>& paths);
		std::vector<std::string> recentPathsStd(size_t max = 10);

		void addRecentPath(QString path);
		void addPath(QString path);

		void setCurrentPath(QString path);
		QString currentPath();

	Q_SIGNALS:
		void currentPathChanged(QString path);

	protected:
		void focusInEvent(QFocusEvent* event);
		void focusOutEvent(QFocusEvent* event);

	private Q_SLOTS:
		void init();
		void onEditTextChanged(QString path);

	private:
		void updateValidStatus();

		bool signalblocked;
		Qt::MatchFlags matchflags;
};

#endif Q_PATH_COMBOBOX_H
