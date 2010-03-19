#include "SettingsManager.h"

#include <fstream>

#include <boost/archive/xml_oarchive.hpp>
#include <boost/archive/xml_iarchive.hpp>
#include <boost/serialization/serialization.hpp>
#include <boost/serialization/map.hpp>

#include <boost/filesystem.hpp>
#include <boost/foreach.hpp>

// editbox
//    - Default
//    - ?Validator

// combobox
//    - Default
//    - List of strings
//    - List of values

// editable combobox
//    - Default
//    - ?Validator
//    - List of strings
//    - List of values

// checkbox
//    - Default

// radio button
//    - Default

// spinbox/time edit
//    - Default
//    - Min
//    - Max

// string: internal name
// string: gui label
// bool: is advanced

// start of test stuff

bool SettingsManagerBase::load(std::map<std::string, std::string>& values)
{
	loadDefaults();

	fixLegacy(values);

	typedef std::pair<std::string, std::string> mp;
	BOOST_FOREACH(const mp& sp, values) {
		if (m_curvalues.count(sp.first))
			m_curvalues[sp.first]->setString(sp.second);
	}
	return true;
}

bool SettingsManagerBase::load(const boost::filesystem::path& path)
{
	bool loaded = false;

	try {
		std::map<std::string, std::string> s_values;
		std::ifstream ifs(path.native_file_string().c_str());
		if (!ifs.is_open()) return false;
		boost::archive::xml_iarchive ia(ifs);
		ia & boost::serialization::make_nvp("settingsmap", s_values);

		loaded = load(s_values);
	} catch (std::exception& e) {
		std::cout << "Load settings failed: " << e.what() << '\n';
		loaded = false;
	}
	return loaded;
}

bool SettingsManagerBase::save(const boost::filesystem::path& path)
{
	try {
		std::map<std::string, std::string> s_values;

		typedef std::pair<std::string, SettingsVariableBase*> mp;
		BOOST_FOREACH(const mp& sp, m_curvalues) {
			s_values[sp.first] = sp.second->getString();
		}

		boost::filesystem::create_directories(path.branch_path());
		std::ofstream ofs(path.native_file_string().c_str());
		if (!ofs.is_open()) return false;
		boost::archive::xml_oarchive oa(ofs);
		oa & boost::serialization::make_nvp("settingsmap", s_values);
		return true;
	} catch (std::exception& e) {
		std::cout << "Load settings failed: " << e.what() << '\n';
		return false;
	}
}

void SettingsManagerBase::loadDefaults()
{
	typedef std::pair<std::string, SettingsInfoRef> mp;
	BOOST_FOREACH(const mp& sp, sinfo) {
		m_curvalues[sp.first]->setString(sp.second->getDefault());
	}
}

class SettingsInfoImpl: public SettingsInfo {
	public:
		std::string def;
		std::vector<std::string> enumstrs;
		std::vector<std::string> enumvals;
		std::string min;
		std::string max;

		virtual std::string getDefault()
		{
			return def;
		}

		virtual std::vector<std::string> getEnumStrings()
		{
			return enumstrs;
		}

		virtual std::vector<std::string> getEnumValues()
		{
			return enumvals;
		}

		virtual std::string valueMin()
		{
			return min;
		}

		virtual std::string valueMax()
		{
			return max;
		}

		virtual bool isValid(const std::string& value)
		{
			return true;
		};
};

SettingsInfoRef createSettingsInfoS(std::string def, std::string enum_str, std::string enum_val, std::string min, std::string max)
{
	boost::shared_ptr<SettingsInfoImpl> s(new SettingsInfoImpl());
	s->def = def;
	return s;
}