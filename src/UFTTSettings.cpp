#include "UFTTSettings.h"

#include <boost/foreach.hpp>

#include "Platform.h"
#include "util/Filesystem.h"

boost::filesystem::path UFTTSettings::getSavePath()
{
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
	return path;
}

void UFTTSettings::registerSettings(SettingsManagerBase* sm)
{
	boost::posix_time::ptime mpt(boost::posix_time::min_date_time);

	sm->registerSettingsVariable("gui.qt.dockinfo", dockinfo, createSettingsInfo(std::vector<uint8>()));

	sm->registerSettingsVariable("gui.qt.posx" , posx , createSettingsInfo(0));
	sm->registerSettingsVariable("gui.qt.posy" , posy , createSettingsInfo(0));
	sm->registerSettingsVariable("gui.qt.sizex", sizex, createSettingsInfo(0));
	sm->registerSettingsVariable("gui.qt.sizey", sizey, createSettingsInfo(0));
	sm->registerSettingsVariable("gui.qt.maximized", guimaximized, createSettingsInfo(false));

	sm->registerSettingsVariable("gui.gtk.showtoolbar", show_toolbar, createSettingsInfo(true));

	sm->registerSettingsVariable("download.path", dl_path, createSettingsInfo(platform::getDownloadPathDefault()));
	sm->registerSettingsVariable("download.resume", experimentalresume, createSettingsInfo(true));

	sm->registerSettingsVariable("update.frompeers", autoupdate, createSettingsInfo(true));
	sm->registerSettingsVariable("update.fromweb", webupdateinterval, createSettingsInfo(2,
		"Never\nDaily\nWeekly\nMonthly",
		"0\n1\n2\n3"
	));
	sm->registerSettingsVariable("update.lastwebupdate", lastupdate, createSettingsInfo(mpt));

	sm->registerSettingsVariable("sharing.globalannounce", enablepeerfinder, createSettingsInfo(true));
	sm->registerSettingsVariable("sharing.nickname", nickname, createSettingsInfo(platform::getUserName()));

	sm->registerSettingsVariable("stun.lastcheck", laststuncheck, createSettingsInfo(mpt));
	sm->registerSettingsVariable("stun.publicip", stunpublicip, createSettingsInfo(""));

	sm->registerSettingsVariable("systray.showtask", show_task_tray_icon, createSettingsInfo(true));
	sm->registerSettingsVariable("systray.minimizetotray", minimize_to_tray, createSettingsInfo(true));
	sm->registerSettingsVariable("systray.closetotray", close_to_tray, createSettingsInfo(true));
	sm->registerSettingsVariable("systray.startintray", start_in_tray, createSettingsInfo(false));
	sm->registerSettingsVariable("systray.doubleclick", traydoubleclick, createSettingsInfo(true));

	sm->registerSettingsVariable("gui.auto_clear_tasks_after", auto_clear_tasks_after, createSettingsInfo(boost::posix_time::seconds(30)));
}

void UFTTSettings::fixLegacy(std::map<std::string, std::string>& v)
{
	if (v.count("systray.minimizemode")) v["systray.closetotray"] = v["systray.minimizemode"];
}
