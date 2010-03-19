#include "DialogPreferences.moc"

#include "../util/SettingsManager.h"

#include <QCheckBox>
#include <QLineEdit>
#include <QTimeEdit>

#include <boost/foreach.hpp>

DialogPreferences::DialogPreferences(QWidget* parent, SettingsManagerBase* settings_)
	: QDialog(parent)
	, settings(settings_)
{
	this->setupUi(this);

	listCategories->setCurrentRow(listCategories->currentRow());

	listCategories->hide();

	delete checkAutoClear;
	delete timeEdit;
	delete horizontalLayout_5;
	delete checkBox_4;
	delete checkBox_5;
	delete checkBox_6;
	delete checkBox_9;

	scanWidgets(this);
}

void DialogPreferences::scanWidgets(QObject* obj)
{
	QVariant settingkey = obj->property("settingkey");
	if (settingkey.isValid()) {
		std::string skey = settingkey.toString().toStdString();
		if (!skey.empty()) widgets[skey] = obj;
	}

	BOOST_FOREACH(QObject* c, obj->children())
		scanWidgets(c);
}

void DialogPreferences::on_buttonBox_clicked(QAbstractButton* button)
{
	QDialogButtonBox::ButtonRole role = buttonBox->buttonRole(button);
	if (role == QDialogButtonBox::AcceptRole || role == QDialogButtonBox::ApplyRole) {
		saveSettings();
	}
}

int DialogPreferences::exec()
{
	loadSettings();
	return QDialog::exec();
}

class Dispatcher {
	private:
		DialogPreferences* dlg;
	public:
		Dispatcher(DialogPreferences* dlg_)
			: dlg(dlg_)
		{
		}

		template <typename T>
		void loadSettings(DialogPreferences::wpair& w)
		{
			T* t = qobject_cast<T*>(w.second);
			if (t) dlg->loadSettings(w.first, t);
		}

		template <typename T>
		void saveSettings(DialogPreferences::wpair& w)
		{
			T* t = qobject_cast<T*>(w.second);
			if (t) dlg->saveSettings(w.first, t);
		}
};

void DialogPreferences::loadSettings()
{
	Dispatcher d(this);
	BOOST_FOREACH(wpair& w, widgets) {
		d.loadSettings<QCheckBox>(w);
		d.loadSettings<QLineEdit>(w);
		d.loadSettings<QTimeEdit>(w);
		d.loadSettings<QComboBox>(w);
	}
}

void DialogPreferences::saveSettings()
{
	Dispatcher d(this);
	BOOST_FOREACH(wpair& w, widgets) {
		d.saveSettings<QCheckBox>(w);
		d.saveSettings<QLineEdit>(w);
		d.saveSettings<QTimeEdit>(w);
		d.saveSettings<QComboBox>(w);
	}
}

// QCheckBox load/save
void DialogPreferences::loadSettings(const std::string& key, QCheckBox* w)
{
	w->setChecked(settings->getVariable(key)->getValue<bool>());
}
void DialogPreferences::saveSettings(const std::string& key, QCheckBox* w)
{
	settings->getVariable(key)->setValue<bool>(w->isChecked());
}

// QLineEdit load/save
void DialogPreferences::loadSettings(const std::string& key, QLineEdit* w)
{
	w->setText(QString::fromStdString(settings->getVariable(key)->getValue<std::string>()));
}
void DialogPreferences::saveSettings(const std::string& key, QLineEdit* w)
{
	settings->getVariable(key)->setValue<std::string>(w->text().toStdString());
}

// QTimeEdit load/save - assumes QTime and boost::posix_time::time_duration have similar text representations
void DialogPreferences::loadSettings(const std::string& key, QTimeEdit* w)
{
	w->setTime(QTime::fromString(QString::fromStdString(settings->getVariable(key)->getString()), "hh:mm:ss"));
}
void DialogPreferences::saveSettings(const std::string& key, QTimeEdit* w)
{
	settings->getVariable(key)->setString(w->time().toString("hh:mm:ss").toStdString());
}

// QComboBox load/save
void DialogPreferences::loadSettings(const std::string& key, QComboBox* w)
{
	if (w->count() == 0) {
		// Initialize
		std::vector<std::string> vtext = settings->getInfo(key)->getEnumStrings();
		std::vector<std::string> vdata = settings->getInfo(key)->getEnumValues();
		for (size_t i = 0; i < vtext.size() && i < vdata.size(); ++i)
			w->addItem(QString::fromStdString(vtext[i]), QString::fromStdString(vdata[i]));
	}
	std::string data = settings->getVariable(key)->getString();
	int idx = w->findData(QString::fromStdString(data));
	w->setCurrentIndex(idx);
}
void DialogPreferences::saveSettings(const std::string& key, QComboBox* w)
{
	settings->getVariable(key)->setString(w->itemData(w->currentIndex()).toString().toStdString());
}
