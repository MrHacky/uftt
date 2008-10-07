#include "UFTTSettings.h"

#include <fstream>
#include <boost/filesystem.hpp>
#include <boost/foreach.hpp>

#include "Platform.h"

#include "../util/Filesystem.h"

const int settings_version = 1;

UFTTSettings::UFTTSettings()
{
	loaded = false;

	posx  = posy  = 0;
	sizex = sizey = 0;

	dl_path = "C:/temp";
	autoupdate = false;
	webupdateinterval = 0;
	lastupdate = boost::posix_time::ptime(boost::posix_time::min_date_time);

	enablepeerfinder = true;
	lastpeerquery = boost::posix_time::ptime(boost::posix_time::min_date_time);
	prevpeerquery = boost::posix_time::ptime(boost::posix_time::min_date_time);
}

bool UFTTSettings::load(boost::filesystem::path path_)
{
	path = path_;
	if (path.empty()) {
		platform::spathlist spl = platform::getSettingsPathList();
		BOOST_FOREACH(const platform::spathinfo& spi, spl) {
			if (!spi.second.empty() && ext::filesystem::exists(spi.second) && boost::filesystem::is_regular(spi.second)) {
				path = spi.second;
				break;
			}
		}

		if (path.empty())
			path = platform::getSettingsPathDefault().second;
	}
	try {
		std::ifstream ifs(path.native_file_string().c_str());
		if (!ifs.is_open()) return false;
		boost::archive::xml_iarchive ia(ifs);
		ia & NVP("settings", *this);
		loaded = true;
	} catch (std::exception& e) {
		std::cout << "Load settings failed: " << e.what() << '\n';
		loaded = false;
	}
	return loaded;
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
