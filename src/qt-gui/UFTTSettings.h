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

	public:
		template<class Archive>
		void serialize(Archive & ar, const unsigned int version) {
			ar & NVP("dockinfo", dockinfo);
			ar & NVP("posx"    , posx);
			ar & NVP("posy"    , posy);
			ar & NVP("sizex"   , sizex);
			ar & NVP("sizey"   , sizey);
		}
};

BOOST_CLASS_VERSION(UFTTSettings, 1)

#endif//UFTT_SETTINGS_H
