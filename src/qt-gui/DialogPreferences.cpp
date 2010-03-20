#include "DialogPreferences.moc"

#include "../util/SettingsManager.h"

#include <QTreeWidgetItem>
#include <QItemDelegate>

#include <boost/foreach.hpp>

namespace {
	enum {
		CN_KEY = 0,
		CN_CURVALUE,
		CN_LASTVALUE,
		CN_DEFAULT,
	};

	class ItemDelegateEditCollumn : public QItemDelegate
	{
		private:
			QTreeWidget* twparent;
			int colnum;

		public:
			ItemDelegateEditCollumn(QTreeWidget* parent_, int colnum_)
				: QItemDelegate(parent_)
				, twparent(parent_)
				, colnum(colnum_)
			{
			}

			QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem& option, const QModelIndex& index) const
			{
				if (index.column() == colnum)
					return QItemDelegate::createEditor(parent, option, index);
				twparent->editItem(twparent->topLevelItem(index.row()), colnum);
				return 0;
			}
	};
}

DialogPreferences::DialogPreferences(QWidget* parent, SettingsManagerBase* settings_)
	: QDialog(parent, Qt::WindowTitleHint | Qt::WindowSystemMenuHint)
	, settings(settings_)
{
	this->setupUi(this);

	delete checkAutoClear;
	delete timeEdit;
	delete horizontalLayout_5;
	delete checkBox_4;
	delete checkBox_5;
	delete checkBox_6;
	delete checkBox_9;

	scanWidgets(this);

	listAdvancedOptions->setItemDelegate(new ItemDelegateEditCollumn(listAdvancedOptions, CN_CURVALUE));
	std::vector<std::string> keys = settings->getAllKeys();
	BOOST_FOREACH(const std::string& s, keys) {
		if (widgets.count(s) == 0) {
			QTreeWidgetItem* twi = new QTreeWidgetItem(listAdvancedOptions);
			twi->setText(CN_KEY,     QString::fromStdString(s));
			twi->setText(CN_DEFAULT, QString::fromStdString(settings->getInfo(s)->getDefault()));
			twi->setFlags(twi->flags() | Qt::ItemIsEditable);
		}
	}

	listCategories->setCurrentRow(0);
}

void DialogPreferences::on_listAdvancedOptions_itemChanged(QTreeWidgetItem* item, int col)
{
	if (col != CN_CURVALUE) return;

	std::string curval = item->text(CN_CURVALUE).toStdString();
	bool isvalid = false;
	try {
		std::string normval = settings->getVariable(item->text(CN_KEY).toStdString())->isValid(curval);
		if (curval != normval) {
			item->setText(CN_CURVALUE, QString::fromStdString(normval));
			return;
		}
		isvalid = true;
	} catch (...) {};

	bool bold = item->text(CN_CURVALUE) != item->text(CN_DEFAULT);
	QFont tfont = item->font(CN_DEFAULT);
	tfont.setBold(bold);
	item->setFont(CN_CURVALUE, tfont);
	item->setFont(CN_KEY     , tfont);

	QColor tcolor = item->textColor(CN_DEFAULT);
	if (!isvalid)
		tcolor = QColor("red");
	item->setTextColor(CN_CURVALUE, tcolor);
	item->setTextColor(CN_KEY     , tcolor);
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

	for (int i = 0; i < listAdvancedOptions->topLevelItemCount (); ++i) {
		QTreeWidgetItem* twi = listAdvancedOptions->topLevelItem(i);
		std::string key = twi->text(CN_KEY).toStdString();
		QString val = QString::fromStdString(settings->getVariable(key)->getString());
		twi->setText(CN_CURVALUE,  val);
		twi->setText(CN_LASTVALUE, val);
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

	for (int i = 0; i < listAdvancedOptions->topLevelItemCount (); ++i) {
		QTreeWidgetItem* twi = listAdvancedOptions->topLevelItem(i);
		std::string key = twi->text(CN_KEY).toStdString();
		if (twi->text(CN_CURVALUE) != twi->text(CN_LASTVALUE)) {
			std::string val = twi->text(CN_CURVALUE).toStdString();
			try {
				settings->getVariable(key)->setString(val);
				twi->setText(CN_LASTVALUE, QString::fromStdString(val));
			} catch (...) {
			}
		}
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
