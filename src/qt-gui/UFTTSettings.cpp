#include "UFTTSettings.h"

#include <fstream>
#include <boost/filesystem.hpp>

const int settings_version = 1;

UFTTSettings::UFTTSettings()
{
	posx  = posy  = 0;
	sizex = sizey = 0;

	dl_path = "C:/temp";
	autoupdate = false;
	webupdateinterval = 0;
	lastupdate = boost::posix_time::ptime(boost::posix_time::min_date_time);
}

bool UFTTSettings::load(boost::filesystem::path path_)
{
	path = path_;
	try {
		std::ifstream ifs(path.native_file_string().c_str());
		if (!ifs.is_open()) return false;
		boost::archive::xml_iarchive ia(ifs);
		ia & NVP("settings", *this);
		return true;
	} catch (std::exception& e) {
		std::cout << "Load settings failed: " << e.what() << '\n';
		return false;
	}
}

bool UFTTSettings::save()
{
	try {
		boost::filesystem::create_directories(path.branch_path());
		std::ofstream ofs(path.native_file_string().c_str());
		if (!ofs.is_open()) return false;
		boost::archive::xml_oarchive oa(ofs);
		oa & NVP("settings", *this);
		return true;
	} catch (std::exception& e) {
		std::cout << "Load settings failed: " << e.what() << '\n';
		return false;
	}
}
