#ifndef DIALOG_DIRECTORY_CHOOSER_H
#define DIALOG_DIRECTORY_CHOOSER_H

#include "ui_DialogDirectoryChooser.h"

#include <boost/filesystem/path.hpp>
#include <boost/foreach.hpp>
#include <vector>

typedef std::pair<std::string, boost::filesystem::path> spathinfo;
typedef std::vector<spathinfo> spathlist;

#include <QDialog>

class DialogDirectoryChooser : public QDialog, public Ui::DialogDirectoryChooser {
	public:
		DialogDirectoryChooser(QWidget* parent = NULL);

		void setPaths(const spathlist& spl);

		boost::filesystem::path getPath();
};

#endif DIALOG_DIRECTORY_CHOOSER_H
