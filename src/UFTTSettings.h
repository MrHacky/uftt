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
	template <typename T>
	struct fromstring_t<std::vector<T> > {
		static void convert(const std::string& in, std::vector<T>& out)
		{
			out.clear();
			std::string sep;
			sep.push_back(0);
			std::vector<std::string> values;
			boost::split(values, in, boost::is_any_of(sep));
			if (values.empty()) return;
			values.pop_back();
			out.resize(values.size());
			for (size_t i = 0; i < values.size(); ++i)
				fromstring(values[i], out[i]);
		}
	};

	template <typename T>
	struct tostring_t<std::vector<T> > {
		static void convert(const std::vector<T>& in, std::string& out)
		{
			out.clear();
			BOOST_FOREACH(const T& t, in) {
				std::string s;
				tostring(t, s);
				out += s.c_str(); // chop off at first NULL
				out.push_back(0);
			}
		}
	};

	template <>
	struct fromstring_t<uint8> {
		static void convert(const std::string& in, uint8& out)
		{
			int v;
			fromstring(in, v);
			out = v;
		}
	};

	template <>
	struct tostring_t<uint8> {
		static void convert(const uint8& in, std::string& out)
		{
			int v = in;
			tostring(v, out);
		}
	};

	template <>
	struct fromstring_t<std::vector<uint8> > {
		static void convert(const std::string& in, std::vector<uint8>& out)
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
	};

	template <>
	struct tostring_t<std::vector<uint8> > {
		static void convert(const std::vector<uint8>& in, std::string& out)
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
	};

	template <>
	struct fromstring_t<ext::filesystem::path> {
		static void convert(const std::string& in, ext::filesystem::path& out)
		{
			out = in;
		}
	};
/*
	template <>
	struct tostring_t<ext::filesystem::path> {
		static void convert(const ext::filesystem::path& in, std::string& out)
		{
			out = in.string();
		}
	};
*/

#ifdef _WIN32_WINDOWS
	// in win9x boost/locales are buggy so use this workaround
	template <>
	struct fromstring_t<boost::posix_time::ptime> {
		static void convert(const std::string& in, boost::posix_time::ptime& out)
		{
			out = boost::posix_time::time_from_string(in);
		}
	};

	template <>
	struct tostring_t<boost::posix_time::ptime> {
		static void convert(const boost::posix_time::ptime& in, std::string& out)
		{
			std::stringstream ss;
			ss << in.date().year() << "-" << in.date().month() << "-" << in.date().day() << " " << in.time_of_day();
			out = ss.str();
		}
	};
#endif
};

class UFTTSettings {
	private:
		ext::filesystem::path path;
	public:
		ext::filesystem::path getSavePath();

		SettingsVariable<int> version;

		SettingsVariable<std::vector<uint8> > dockinfo;
		SettingsVariable<int> posx;
		SettingsVariable<int> posy;
		SettingsVariable<int> sizex;
		SettingsVariable<int> sizey;
		SettingsVariable<bool> guimaximized;

		SettingsVariable<std::vector<std::string> > recentdownloadpaths;

		SettingsVariable<bool> showadvancedsettings;

		SettingsVariable<ext::filesystem::path> dl_path;
		SettingsVariable<ext::filesystem::path> update_path;
		SettingsVariable<bool> autoupdate;
		SettingsVariable<bool> experimentalresume;
		SettingsVariable<bool> traydoubleclick;

		SettingsVariable<int> webupdateinterval; // 0:never, 1:daily, 2:weekly, 3:monthly
		SettingsVariable<boost::posix_time::ptime> lastupdate;

		SettingsVariable<int> minbuildtype;

		SettingsVariable<bool> enablepeerfinder;
		SettingsVariable<std::string> nickname;

		SettingsVariable<boost::posix_time::ptime> laststuncheck;
		SettingsVariable<std::string> stunpublicip;

		SettingsVariable<bool> show_task_tray_icon;
		SettingsVariable<bool> minimize_to_tray;
		SettingsVariable<bool> close_to_tray;
		SettingsVariable<bool> start_in_tray;

		SettingsVariable<bool> uftt_send_to;
		SettingsVariable<bool> uftt_desktop_shortcut;
		SettingsVariable<bool> uftt_quicklaunch_shortcut;
		SettingsVariable<bool> uftt_startmenu_group;

		SettingsVariable<bool> notification_on_completion;
		SettingsVariable<bool> blink_statusicon_on_completion;
		SettingsVariable<bool> show_speeds_in_titlebar;
		SettingsVariable<bool> show_speeds_in_statusicon_tooltip;

		/* Gtk GUI */
		SettingsVariable<bool> show_toolbar;

		// Automatically clear completed tasks from the tasklist after
		// this amount of time. Any negative duration indicates that
		// the option is disabled.
		SettingsVariable<boost::posix_time::time_duration> auto_clear_tasks_after;

		SettingsVariable<bool> showautoupdates;

		void registerSettings(SettingsManagerBase* sm);
		void fixLegacy(std::map<std::string, std::string>& v);
};

typedef SettingsManagerRef<UFTTSettings> UFTTSettingsRef;

#endif // UFTT_SETTINGS_H
