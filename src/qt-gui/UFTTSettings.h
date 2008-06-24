#ifndef UFTT_SETTINGS_H
#define UFTT_SETTINGS_H

#include "../Types.h"

#include <vector>

#include <boost/filesystem/path.hpp>

#include <boost/archive/xml_oarchive.hpp>
#include <boost/archive/xml_iarchive.hpp>
#include <boost/serialization/serialization.hpp>
#include <boost/serialization/vector.hpp>
#include <boost/serialization/version.hpp>

#define NVP(x,y) (boost::serialization::make_nvp(x,y))


class UFTTSettings {
	public:
		UFTTSettings();

		bool load(boost::filesystem::path path_);
		bool save();

	public:
		boost::filesystem::path path;

		std::vector<uint8> dockinfo;
		int posx;
		int posy;
		int sizex;
		int sizey;
		boost::filesystem::path dl_path;
		bool autoupdate;

	public:
		template<class Archive>
		void serialize(Archive & ar, const unsigned int version) {
			ar & NVP("dockinfo", dockinfo);
			ar & NVP("posx"    , posx);
			ar & NVP("posy"    , posy);
			ar & NVP("sizex"   , sizex);
			ar & NVP("sizey"   , sizey);
			if (version >=  2) ar & NVP("downloadpath", dl_path);
			if (version >=  3) ar & NVP("autoupdate"  , autoupdate);
		}
};

BOOST_CLASS_VERSION(UFTTSettings, 3)

///////////////////////////////////////////////////////////////////////////////
//  Serialization support for boost::filesystem::path
namespace boost { namespace serialization {

	template<class Archive>
inline void save (Archive & ar, boost::filesystem::path const& p, 
    const unsigned int /* file_version */)
{
    using namespace boost::serialization;
    std::string path_str(p.string());
    ar & make_nvp("path", path_str);
}

template<class Archive>
inline void load (Archive & ar, boost::filesystem::path &p,
    const unsigned int /* file_version */)
{
    using namespace boost::serialization;
    std::string path_str;
    ar & make_nvp("path", path_str);
    p = boost::filesystem::path(path_str);//, boost::filesystem::native);
}

// split non-intrusive serialization function member into separate
// non intrusive save/load member functions
template<class Archive>
inline void serialize (Archive & ar, boost::filesystem::path &p,
    const unsigned int file_version)
{
    boost::serialization::split_free(ar, p, file_version);
}

} } // namespace boost::serializatio

#endif//UFTT_SETTINGS_H
