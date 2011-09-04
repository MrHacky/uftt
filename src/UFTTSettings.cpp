#include "UFTTSettings.h"

#include <boost/foreach.hpp>

#include "Platform.h"
#include "util/Filesystem.h"

ext::filesystem::path UFTTSettings::getSavePath()
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

	sm->registerSettingsVariable("gui.qt.dockinfo", dockinfo, std::vector<uint8>());

	sm->registerSettingsVariable("gui.qt.posx" , posx , 0);
	sm->registerSettingsVariable("gui.qt.posy" , posy , 0);
	sm->registerSettingsVariable("gui.qt.sizex", sizex, 0);
	sm->registerSettingsVariable("gui.qt.sizey", sizey, 0);
	sm->registerSettingsVariable("gui.qt.maximized", guimaximized, false);

	sm->registerSettingsVariable("gui.qt.showadvancedsettings", showadvancedsettings, false);

	sm->registerSettingsVariable("gui.gtk.showtoolbar", show_toolbar, true);

	sm->registerSettingsVariable("gui.recentdownloadpaths", recentdownloadpaths, std::vector<std::string>());

	sm->registerSettingsVariable("download.path", dl_path, platform::getDownloadPathDefault());
	sm->registerSettingsVariable("download.resume", experimentalresume, true);
	sm->registerSettingsVariable("download.notification_on_completion", notification_on_completion, true);
	sm->registerSettingsVariable("download.blink_statusicon_on_completion", blink_statusicon_on_completion, true);
	sm->registerSettingsVariable("download.show_speeds_in_titlebar", show_speeds_in_titlebar, true);
	sm->registerSettingsVariable("download.show_speeds_in_statusicon_tooltip", show_speeds_in_statusicon_tooltip, true);

	sm->registerSettingsVariable("update.frompeers", autoupdate, true);
	sm->registerSettingsVariable("update.fromweb", webupdateinterval, 2,
		"Never\nDaily\nWeekly\nMonthly",
		"0\n1\n2\n3"
	);
	sm->registerSettingsVariable("update.lastwebupdate", lastupdate, mpt);

	sm->registerSettingsVariable("sharing.globalannounce", enablepeerfinder, true);
	sm->registerSettingsVariable("sharing.nickname", nickname, platform::getUserName());

	sm->registerSettingsVariable("stun.lastcheck", laststuncheck, mpt);
	sm->registerSettingsVariable("stun.publicip", stunpublicip, "");

	sm->registerSettingsVariable("systray.showtask", show_task_tray_icon, true);
	sm->registerSettingsVariable("systray.minimizetotray", minimize_to_tray, true);
	sm->registerSettingsVariable("systray.closetotray", close_to_tray, false);
	sm->registerSettingsVariable("systray.startintray", start_in_tray, false);
	sm->registerSettingsVariable("systray.doubleclick", traydoubleclick, true);

	// Don't actually do anything, disabled in UFTTCore.cpp
	sm->registerSettingsVariable("system.sendtouftt", uftt_send_to, false);
	sm->registerSettingsVariable("system.desktopshortcut", uftt_desktop_shortcut, false);
	sm->registerSettingsVariable("system.quicklaunchshortcut", uftt_quicklaunch_shortcut, false);
	sm->registerSettingsVariable("system.startmenugroup", uftt_startmenu_group, false);

	sm->registerSettingsVariable("gui.auto_clear_tasks_after", auto_clear_tasks_after, boost::posix_time::seconds(30));

	sm->registerSettingsVariable("update.showshares", showautoupdates, false);
}

void UFTTSettings::fixLegacy(std::map<std::string, std::string>& v)
{
}
