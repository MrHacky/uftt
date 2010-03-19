#ifndef UFTT_SETTINGS_H
#define UFTT_SETTINGS_H

#include "util/SettingsManager.h"

#include <vector>

#include "Types.h"
#include <boost/filesystem/path.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/date_time/posix_time/time_serialize.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/foreach.hpp>


namespace settingsmanager {
	template <>
	inline void fromstring(const std::string& in, uint8& out)
	{
		int v;
		fromstring(in, v);
		out = v;
	}

	template <>
	inline void tostring(const uint8& in, std::string& out)
	{
		int v = in;
		tostring(v, out);
	}

	template <>
	inline void fromstring(const std::string& in, std::vector<uint8>& out)
	{
		if (in == "") {
			out.clear();
		} else {
			std::vector<std::string> values;
			boost::split(values, in, boost::is_any_of(","));
			out.resize(values.size());
			for (size_t i = 0; i < values.size(); ++i)
				fromstring(values[i], out[i]);
		}
	}

	template <>
	inline void tostring(const std::vector<uint8>& in, std::string& out)
	{
		out.clear();
		if (!in.empty())
			tostring(in[0], out);

		for (size_t i = 1; i < in.size(); ++i) {
			std::string t;
			tostring(in[i], t);
			out += ",";
			out += t;
		}
	}

	template <>
	inline void fromstring(const std::string& in, boost::filesystem::path& out)
	{
		out = in;
	}
/*
	template <>
	inline void tostring(const boost::filesystem::path& in, std::string& out)
	{
		out = in.string();
	}
*/
};

class UFTTSettings {
	private:
		boost::filesystem::path path;
	public:
		boost::filesystem::path getSavePath();

		SettingsVariable<int> version;

		SettingsVariable<std::vector<uint8> > dockinfo;
		SettingsVariable<int> posx;
		SettingsVariable<int> posy;
		SettingsVariable<int> sizex;
		SettingsVariable<int> sizey;

		SettingsVariable<boost::filesystem::path> dl_path;
		SettingsVariable<bool> autoupdate;
		SettingsVariable<bool> experimentalresume;
		SettingsVariable<bool> traydoubleclick;

		SettingsVariable<int> webupdateinterval; // 0:never, 1:daily, 2:weekly, 3:monthly
		SettingsVariable<boost::posix_time::ptime> lastupdate;

		SettingsVariable<bool> enablepeerfinder;
		SettingsVariable<std::string> nickname;

		SettingsVariable<boost::posix_time::ptime> laststuncheck;
		SettingsVariable<std::string> stunpublicip;

		/* Gtk GUI */
		SettingsVariable<bool> show_toolbar;
		SettingsVariable<bool> show_task_tray_icon;
		// 0 = Do not minimize to tray,
		// 1 = Minimize to tray on close,
		// 2 = Minimize to tray on minimize.
		// Option 2 may not be implemented for all platforms/gui combinations.
		SettingsVariable<int>  minimize_to_tray_mode;
		SettingsVariable<bool> start_in_tray;

		// Automatically clear completed tasks from the tasklist after
		// this amount of time. Any negative duration indicates that
		// the option is disabled.
		SettingsVariable<boost::posix_time::time_duration> auto_clear_tasks_after;

		void registerSettings(SettingsManagerBase* sm);
		void fixLegacy(std::map<std::string, std::string>& v);
};

typedef SettingsManagerRef<UFTTSettings> UFTTSettingsRef;

#endif // UFTT_SETTINGS_H
