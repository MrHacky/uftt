#ifndef SIMPLE_BACKEND_BASE_H
#define SIMPLE_BACKEND_BASE_H

#include <string>

#include <boost/filesystem.hpp>
#include <boost/asio.hpp>

#include "INetModule.h"
#include "../UFTTSettings.h"

class SimpleBackendBase: public INetModule {
	public:
		std::set<boost::asio::ip::address> foundpeers;

		virtual boost::filesystem::path getsharepath(std::string name) = 0;
		virtual UFTTSettings& getSettings() = 0;

		std::vector<boost::asio::ip::address> get_broadcast_adresses();
};

#endif//SIMPLE_BACKEND_BASE_H
