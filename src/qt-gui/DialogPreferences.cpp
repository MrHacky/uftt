#include "DialogPreferences.h"

#include "../util/SettingsManager.h"

#include <QTreeWidgetItem>
#include <QItemDelegate>
#include <QPushButton>

#include <boost/foreach.hpp>

#include "QExtUTF8.h"

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

DialogPreferences::DialogPreferences(boost::shared_ptr<SettingsManagerBase> settings_, QWidget* parent)
	: QDialog(parent, Qt::WindowTitleHint | Qt::WindowSystemMenuHint)
	, settings(settings_)
{
	this->setupUi(this);

	delete checkAutoClear;
	delete timeEdit;
	delete horizontalLayout_5;
	delete checkBox_6;

	scanWidgets(this);

	listAdvancedOptions->setItemDelegate(new ItemDelegateEditCollumn(listAdvancedOptions, CN_CURVALUE));
	std::vector<std::string> keys = settings->getAllKeys();
	BOOST_FOREACH(const std::string& s, keys) {
		if (widgets.count(s) == 0) {
			QTreeWidgetItem* twi = new QTreeWidgetItem(listAdvancedOptions);
			twi->setText(CN_KEY,     S2Q(s));
			twi->setText(CN_DEFAULT, S2Q(settings->getInfo(s)->getDefault()));
			twi->setFlags(twi->flags() | Qt::ItemIsEditable);
		}
	}

	listCategories->setCurrentRow(0);

	SettingsVariableBase* sv = settings->getVariable("gui.qt.showadvancedsettings");
	bool showadvanced = sv->getValue<bool>();
	if (!showadvanced)
		delete listCategories->item(listCategories->count()-1);

	initSettings();
}

void DialogPreferences::on_listAdvancedOptions_itemChanged(QTreeWidgetItem* item, int col)
{
	if (col != CN_CURVALUE) return;
	onSettingChanged();

	std::string curval = Q2S(item->text(CN_CURVALUE));
	bool isvalid = false;
	try {
		std::string normval = settings->getVariable(Q2S(item->text(CN_KEY)))->isValid(curval);
		if (curval != normval) {
			item->setText(CN_CURVALUE, S2Q(normval));
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
		std::string skey = Q2S(settingkey.toString());
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

void DialogPreferences::on_line_customContextMenuRequested(QPoint)
{
	SettingsVariableBase* sv = settings->getVariable("gui.qt.showadvancedsettings");
	bool showadvanced = sv->getValue<bool>();
	showadvanced = !showadvanced;
	sv->setValue<bool>(showadvanced);
	bool isshowing = listCategories->count() == stackedCategories->count();
	if (showadvanced == isshowing) return;
	if (showadvanced)
		listCategories->addItem("Advanced");
	else
		delete listCategories->item(listCategories->count()-1);
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

		template <typename T>
		void initSettings(DialogPreferences::wpair& w)
		{
			T* t = qobject_cast<T*>(w.second);
			if (t) dlg->initSettings(w.first, t);
		}
};

void DialogPreferences::initSettings()
{
	Dispatcher d(this);
	BOOST_FOREACH(wpair& w, widgets) {
		d.initSettings<QCheckBox>(w);
		d.initSettings<QGroupBox>(w);
		d.initSettings<QLineEdit>(w);
		d.initSettings<QTimeEdit>(w);
		d.initSettings<QComboBox>(w);
	}
}

void DialogPreferences::loadSettings()
{
	Dispatcher d(this);
	BOOST_FOREACH(wpair& w, widgets) {
		d.loadSettings<QCheckBox>(w);
		d.loadSettings<QGroupBox>(w);
		d.loadSettings<QLineEdit>(w);
		d.loadSettings<QTimeEdit>(w);
		d.loadSettings<QComboBox>(w);
	}

	for (int i = 0; i < listAdvancedOptions->topLevelItemCount (); ++i) {
		QTreeWidgetItem* twi = listAdvancedOptions->topLevelItem(i);
		std::string key = Q2S(twi->text(CN_KEY));
		QString val = S2Q(settings->getVariable(key)->getString());
		twi->setText(CN_CURVALUE,  val);
		twi->setText(CN_LASTVALUE, val);
	}

	buttonBox->button(QDialogButtonBox::Apply)->setEnabled(false);
}

void DialogPreferences::saveSettings()
{
	Dispatcher d(this);
	BOOST_FOREACH(wpair& w, widgets) {
		d.saveSettings<QCheckBox>(w);
		d.saveSettings<QGroupBox>(w);
		d.saveSettings<QLineEdit>(w);
		d.saveSettings<QTimeEdit>(w);
		d.saveSettings<QComboBox>(w);
	}

	for (int i = 0; i < listAdvancedOptions->topLevelItemCount (); ++i) {
		QTreeWidgetItem* twi = listAdvancedOptions->topLevelItem(i);
		std::string key = Q2S(twi->text(CN_KEY));
		if (twi->text(CN_CURVALUE) != twi->text(CN_LASTVALUE)) {
			std::string val = Q2S(twi->text(CN_CURVALUE));
			try {
				settings->getVariable(key)->setString(val);
				twi->setText(CN_LASTVALUE, S2Q(val));
			} catch (...) {
			}
		}
	}

	buttonBox->button(QDialogButtonBox::Apply)->setEnabled(false);
}

void DialogPreferences::onSettingChanged()
{
	buttonBox->button(QDialogButtonBox::Apply)->setEnabled(true);
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
void DialogPreferences::initSettings(const std::string& key, QCheckBox* w)
{
	QObject::connect(w, SIGNAL(toggled(bool)), this, SLOT(onSettingChanged()));
}

// QGroupBox load/save
void DialogPreferences::loadSettings(const std::string& key, QGroupBox* w)
{
	w->setChecked(settings->getVariable(key)->getValue<bool>());
}
void DialogPreferences::saveSettings(const std::string& key, QGroupBox* w)
{
	settings->getVariable(key)->setValue<bool>(w->isChecked());
}
void DialogPreferences::initSettings(const std::string& key, QGroupBox* w)
{
	QObject::connect(w, SIGNAL(toggled(bool)), this, SLOT(onSettingChanged()));
}

// QLineEdit load/save
void DialogPreferences::loadSettings(const std::string& key, QLineEdit* w)
{
	w->setText(qext::utf8::toQString(settings->getVariable(key)->getValue<std::string>()));
}
void DialogPreferences::saveSettings(const std::string& key, QLineEdit* w)
{
	settings->getVariable(key)->setValue<std::string>(qext::utf8::fromQString(w->text()));
}
void DialogPreferences::initSettings(const std::string& key, QLineEdit* w)
{
	QObject::connect(w, SIGNAL(textChanged(QString)), this, SLOT(onSettingChanged()));
}

// QTimeEdit load/save - assumes QTime and boost::posix_time::time_duration have similar text representations
void DialogPreferences::loadSettings(const std::string& key, QTimeEdit* w)
{
	w->setTime(QTime::fromString(S2Q(settings->getVariable(key)->getString()), "hh:mm:ss"));
}
void DialogPreferences::saveSettings(const std::string& key, QTimeEdit* w)
{
	settings->getVariable(key)->setString(Q2S(w->time().toString("hh:mm:ss")));
}
void DialogPreferences::initSettings(const std::string& key, QTimeEdit* w)
{
	QObject::connect(w, SIGNAL(timeChanged(QTime)), this, SLOT(onSettingChanged()));
}

// QComboBox load/save
void DialogPreferences::loadSettings(const std::string& key, QComboBox* w)
{
	std::string data = settings->getVariable(key)->getString();
	int idx = w->findData(S2Q(data));
	w->setCurrentIndex(idx);
}
void DialogPreferences::saveSettings(const std::string& key, QComboBox* w)
{
	settings->getVariable(key)->setString(Q2S(w->itemData(w->currentIndex()).toString()));
}
void DialogPreferences::initSettings(const std::string& key, QComboBox* w)
{
	std::vector<std::string> vtext = settings->getInfo(key)->getEnumStrings();
	std::vector<std::string> vdata = settings->getInfo(key)->getEnumValues();
	for (size_t i = 0; i < vtext.size() && i < vdata.size(); ++i)
		w->addItem(S2Q(vtext[i]), S2Q(vdata[i]));

	QObject::connect(w, SIGNAL(currentIndexChanged(int)), this, SLOT(onSettingChanged()));
}
