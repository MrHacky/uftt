#include "UFTTSettingsLegacy.h"

#ifdef ENABLE_LEGACY_SETTINGS

#include "util/Filesystem.h"

#include "Types.h"

#include <vector>
#include <set>

#include <boost/filesystem/path.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/date_time/posix_time/time_serialize.hpp>

#include <boost/archive/xml_oarchive.hpp>
#include <boost/archive/xml_iarchive.hpp>
#include <boost/serialization/serialization.hpp>
#include <boost/serialization/vector.hpp>
#include <boost/serialization/set.hpp>
#include <boost/serialization/version.hpp>

#define NVP(x,y) (boost::serialization::make_nvp(x,y))

struct vector_as_string {
	std::vector<uint8>& data;

	vector_as_string(std::vector<uint8>& data_)
		: data(data_) {};

	template<class Archive>
	void save(Archive & ar, const unsigned int version) const {
		std::string strdata(data.begin(), data.end());
		ar & NVP("data", strdata);
	}

	template<class Archive>
	void load(Archive & ar, const unsigned int version) const {
		std::string strdata;
		ar & NVP("data", strdata);
		data = std::vector<uint8>(strdata.begin(), strdata.end());
	}

	template<class Archive>
	void serialize(Archive & ar, const unsigned int version) const {
		boost::serialization::split_member(ar, *this, version);
	}
};

class UFTTSettingsLegacy {
	public:
		UFTTSettingsLegacy();

		bool load(ext::filesystem::path path_ = "");
		bool save();

	public:
		ext::filesystem::path path;
		bool loaded;

		std::vector<uint8> dockinfo;
		int posx;
		int posy;
		int sizex;
		int sizey;

		ext::filesystem::path dl_path;
		bool autoupdate;
		bool experimentalresume;
		bool traydoubleclick;

		int webupdateinterval; // 0:never, 1:daily, 2:weekly, 3:monthly
		boost::posix_time::ptime lastupdate;

		bool enablepeerfinder;
		std::string nickname;

		boost::posix_time::ptime laststuncheck;
		std::string stunpublicip;

		/* Gtk GUI */
		bool show_toolbar;
		bool show_task_tray_icon;
		// 0 = Do not minimize to tray,
		// 1 = Minimize to tray on close,
		// 2 = Minimize to tray on minimize.
		// Option 2 may not be implemented for all platforms/gui combinations.
		int  minimize_to_tray_mode;
		bool start_in_tray;

		// Automatically clear completed tasks from the tasklist after
		// this amount of time. Any negative duration indicates that
		// the option is disabled.
		boost::posix_time::time_duration auto_clear_tasks_after;

	public:
		template<class Archive>
		void serialize(Archive & ar, const unsigned int version) {
			if (version  <  4) ar & NVP("dockinfo", dockinfo);
			ar & NVP("posx"    , posx);
			ar & NVP("posy"    , posy);
			ar & NVP("sizex"   , sizex);
			ar & NVP("sizey"   , sizey);
			if (version >=  2) ar & NVP("downloadpath", dl_path);
			if (version >=  3) ar & NVP("autoupdate"  , autoupdate);
			if (version >=  5) ar & NVP("updateinterval", webupdateinterval);
			if (version >=  6) ar & NVP("peerfinder", enablepeerfinder);
			if (version >=  7) ar & NVP("experimentalresume", experimentalresume);
			if (version >=  8) ar & NVP("nickname", nickname);
			if (version >= 11) ar & NVP("traydoubleclick", traydoubleclick);

			if (version >=  5) ar & NVP("lastupdate", lastupdate);
			if (version >=  6 && version <=  8) {
				boost::posix_time::ptime lastpeerquery;
				boost::posix_time::ptime prevpeerquery;
				std::set<std::string> foundpeers;
				ar & NVP("lastpeerquery", lastpeerquery);
				ar & NVP("prevpeerquery", prevpeerquery);
				ar & NVP("foundpeers", foundpeers);
			}
			if (version >=  4) ar & NVP("dockinfo", dockinfo);

			if (version <=  9) { // force new defaults for old versions
				experimentalresume = true;
				autoupdate = true;
			}

			//if (version >=  4 && version < 6) ar & NVP("dockinfo", dockinfo);
			//if (version >=  6) ar & NVP("dockinfo", vector_as_string(dockinfo));

			/* Gtk GUI */
			if (version >= 12) ar & NVP("show_toolbar", show_toolbar);
			if (version >= 13) ar & NVP("show_task_tray_icon", show_task_tray_icon);
			if (version >= 13) ar & NVP("minimize_to_tray_mode", minimize_to_tray_mode);
			if (version >= 13) ar & NVP("start_in_tray", start_in_tray);
			if (version >= 14) ar & NVP("auto_clear_tasks_after", auto_clear_tasks_after);
		}
};
//typedef boost::shared_ptr<UFTTSettings> UFTTSettingsRef;

BOOST_CLASS_VERSION(UFTTSettingsLegacy, 14)

///////////////////////////////////////////////////////////////////////////////
//  Serialization support for ext::filesystem::path
namespace boost { namespace serialization {

	template<class Archive>
inline void save (Archive & ar, ::ext::filesystem::path const& p,
    const unsigned int /* file_version */)
{
    using namespace boost::serialization;
    std::string path_str(p.string());
    ar & make_nvp("path", path_str);
}

template<class Archive>
inline void load (Archive & ar, ::ext::filesystem::path &p,
    const unsigned int /* file_version */)
{
    using namespace boost::serialization;
    std::string path_str;
    ar & make_nvp("path", path_str);
    p = ::ext::filesystem::path(path_str);//, boost::filesystem::native);
}

// split non-intrusive serialization function member into separate
// non intrusive save/load member functions
template<class Archive>
inline void serialize (Archive & ar, ::ext::filesystem::path &p,
    const unsigned int file_version)
{
    boost::serialization::split_free(ar, p, file_version);
}

} } // namespace boost::serialization

///////////////////////////////////
// END OF HEADER - START OF CODE //
///////////////////////////////////

#include <fstream>
#include <boost/filesystem.hpp>
#include <boost/foreach.hpp>

#include "Platform.h"

#include "util/Filesystem.h"

const int settings_version = 1;

UFTTSettingsLegacy::UFTTSettingsLegacy()
{
	loaded = false;

	posx  = posy  = 0;
	sizex = sizey = 0;

	dl_path = platform::getDownloadPathDefault();
	autoupdate = true;
	webupdateinterval = 2; // default to weekly update checks
	lastupdate = boost::posix_time::ptime(boost::posix_time::min_date_time);

	enablepeerfinder = true;
	laststuncheck = boost::posix_time::ptime(boost::posix_time::min_date_time);

	nickname = platform::getUserName();

	experimentalresume = true;

	traydoubleclick = true;

	/* Gtk GUI */
	show_toolbar = true;
	show_task_tray_icon = true;
	minimize_to_tray_mode = 1;
	start_in_tray = false;

	auto_clear_tasks_after = boost::posix_time::seconds(30); // default to clearing tasks after 30 seconds
}

bool UFTTSettingsLegacy::load(ext::filesystem::path path_)
{
	path = path_;
	try {
		ext::filesystem::ifstream ifs(path);
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

bool UFTTSettingsLegacy::save()
{
	try {
		boost::filesystem::create_directories(path.parent_path());
		ext::filesystem::ofstream ofs(path);
		if (!ofs.is_open()) return false;
		boost::archive::xml_oarchive oa(ofs);
		oa & NVP("settings", *this);
		return true;
	} catch (std::exception& e) {
		std::cout << "Load settings failed: " << e.what() << '\n';
		return false;
	}
}

namespace {
	template <typename T>
	std::string getString(const T& t)
	{
		std::string s;
		settingsmanager::tostring(t, s);
		return s;
	}
}

#endif//ENABLE_LEGACY_SETTINGS

bool UFTTSettingsLegacyLoader(UFTTSettingsRef sref)
{
#ifdef ENABLE_LEGACY_SETTINGS
	try {
		ext::filesystem::path path = sref.get()->s_curvalues.getSavePath();
		if (!ext::filesystem::exists(path)) return false;

		UFTTSettingsLegacy legacy;
		legacy.load(path);
		if (!legacy.loaded) return false;

		std::map<std::string, std::string> s_values;
		s_values["gui.qt.dockinfo"] = getString(legacy.dockinfo);

		s_values["gui.qt.posx"] = getString(legacy.posx);
		s_values["gui.qt.posy"] = getString(legacy.posy);
		s_values["gui.qt.sizex"] = getString(legacy.sizex);
		s_values["gui.qt.sizey"] = getString(legacy.sizey);

		s_values["gui.gtk.showtoolbar"] = getString(legacy.show_toolbar);

		s_values["download.path"] = getString(legacy.dl_path);
		s_values["download.resume"] = getString(legacy.experimentalresume);

		s_values["update.frompeers"] = getString(legacy.autoupdate);
		s_values["update.fromweb"] = getString(legacy.webupdateinterval);
		s_values["update.lastwebupdate"] = getString(legacy.lastupdate);


		s_values["sharing.globalannounce"] = getString(legacy.enablepeerfinder);
		s_values["sharing.nickname"] = getString(legacy.nickname);

		s_values["stun.lastcheck"] = getString(legacy.laststuncheck);
		s_values["stun.publicip"] = getString(legacy.stunpublicip);

		s_values["systray.showtask"] = getString(legacy.show_task_tray_icon);
		s_values["systray.minimizemode"] = getString(legacy.minimize_to_tray_mode);
		s_values["systray.startintray"] = getString(legacy.start_in_tray);
		s_values["systray.doubleclick"] = getString(legacy.traydoubleclick);

		s_values["gui.auto_clear_tasks_after"] = getString(legacy.auto_clear_tasks_after);

		return sref.get()->load(s_values);
	} catch (...) {}
#endif//ENABLE_LEGACY_SETTINGS
	return false;
}
