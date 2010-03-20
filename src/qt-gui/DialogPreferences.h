#ifndef DIALOG_PREFERENCES_H
#define DIALOG_PREFERENCES_H

#include "ui_DialogPreferences.h"

#include <map>

#include <QDialog>

class SettingsManagerBase;

class DialogPreferences: public QDialog, public Ui::DialogPreferences {
	Q_OBJECT

	private:
		friend class Dispatcher;

		std::map<std::string, QObject*> widgets;
		typedef std::pair<const std::string, QObject*> wpair;

		SettingsManagerBase* settings;

		void scanWidgets(QObject* obj);

		void loadSettings();
		void loadSettings(const std::string& key, QCheckBox* w);
		void loadSettings(const std::string& key, QLineEdit* w);
		void loadSettings(const std::string& key, QTimeEdit* w);
		void loadSettings(const std::string& key, QComboBox* w);

		void saveSettings();
		void saveSettings(const std::string& key, QCheckBox* w);
		void saveSettings(const std::string& key, QLineEdit* w);
		void saveSettings(const std::string& key, QTimeEdit* w);
		void saveSettings(const std::string& key, QComboBox* w);

	public:
		DialogPreferences(QWidget* parent, SettingsManagerBase* settings_);

	public Q_SLOTS:
		void on_buttonBox_clicked(QAbstractButton*);
		void on_listAdvancedOptions_itemChanged(QTreeWidgetItem*, int);

		int exec();
};

#endif//DIALOG_PREFERENCES_H
