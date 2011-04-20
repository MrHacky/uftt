#include "DialogDirectoryChooser.h"

#include "QExtUTF8.h"

DialogDirectoryChooser::DialogDirectoryChooser(QWidget* parent)
	: QDialog(parent)
{
	this->setupUi(this);
}

void DialogDirectoryChooser::setPaths(const spathlist& spl) {
	BOOST_FOREACH(const spathinfo& spi, spl) {
		QStringList sl;
		sl << S2Q(spi.first) << qext::path::toQStringDirectory(spi.second);
		new QTreeWidgetItem(listPaths, sl);
	}
	listPaths->resizeColumnToContents(0);
	listPaths->resizeColumnToContents(1);
}

ext::filesystem::path DialogDirectoryChooser::getPath()
{
	QList<QTreeWidgetItem*> selected = listPaths->selectedItems();

	BOOST_FOREACH(QTreeWidgetItem* wi, selected) {
		return Q2S(wi->text(1));
	}
	return "";
}
