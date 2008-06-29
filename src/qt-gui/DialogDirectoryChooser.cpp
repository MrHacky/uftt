#include "DialogDirectoryChooser.h"

DialogDirectoryChooser::DialogDirectoryChooser(QWidget* parent)
	: QDialog(parent)
{
	this->setupUi(this);
}

void DialogDirectoryChooser::setPaths(const spathlist& spl) {
	BOOST_FOREACH(const spathinfo& spi, spl) {
		QStringList sl;
		sl << QString::fromStdString(spi.first) << QString::fromStdString(spi.second.native_file_string());
		new QTreeWidgetItem(listPaths, sl);
	}
	listPaths->resizeColumnToContents(0);
	listPaths->resizeColumnToContents(1);
}

boost::filesystem::path DialogDirectoryChooser::getPath()
{
	QList<QTreeWidgetItem*> selected = listPaths->selectedItems();

	BOOST_FOREACH(QTreeWidgetItem* wi, selected) {
		return wi->text(1).toStdString();
	}
	return "";
}
