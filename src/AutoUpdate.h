#ifndef AUTO_UPDATE_H
#define AUTO_UPDATE_H

#include "Types.h"
#include <boost/asio.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/signal.hpp>
#include "net-asio/asio_file_stream.h"

class AutoUpdater {
	public: // helper functions for the autoupdate process

		static int replace(const boost::filesystem::path& source, const boost::filesystem::path& target);

		static void remove(boost::asio::io_service& result_service, boost::asio::io_service& work_service, const boost::filesystem::path& target);

		static bool isBuildBetter(const std::string& newstr, const std::string& oldstr);

		static bool doSelfUpdate(const std::string& buildname, const boost::filesystem::path& target, const boost::filesystem::path& selfpath);

		static std::vector<std::pair<std::string, std::string> > parseUpdateWebPage(const std::vector<uint8>& webpage);

		static bool doSigning(const boost::filesystem::path& keyfile, const std::string& build, const boost::filesystem::path& infile, const boost::filesystem::path& outfile);

	public: // actual object interface for offering builds for autoupdate

		void checkfile(services::diskio_service& disk_service, boost::asio::io_service& result_service, boost::asio::io_service& work_service, const boost::filesystem::path& target, const std::string& bstring, bool signifneeded = false);

		boost::shared_ptr<std::vector<uint8> > getBuildExecutable(const std::string& buildname) const;

		const std::vector<std::string>& getAvailableBuilds() const;

		boost::signal<void(std::string)> newbuild;

	private:
		void addBuild(std::string, shared_vec data);

		std::vector<std::string> buildstrings;
		std::map<std::string, boost::shared_ptr<std::vector<uint8> > > filedata;
};

#endif//AUTO_UPDATE_H
