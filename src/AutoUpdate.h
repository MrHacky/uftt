#ifndef AUTO_UPDATE_H
#define AUTO_UPDATE_H

#include "Types.h"
#include <boost/asio.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/signals2/signal.hpp>
#include "net-asio/asio_file_stream.h"

static const char AutoUpdaterTagData[] = "[\000uftt\000-\000update\000]";
static const std::string AutoUpdaterTag(AutoUpdaterTagData, AutoUpdaterTagData + sizeof(AutoUpdaterTagData));

class AutoUpdater {
	public: // helper functions for the autoupdate process

		static int replace(const ext::filesystem::path& source, const ext::filesystem::path& target);

		static void remove(boost::asio::io_service& result_service, boost::asio::io_service& work_service, const ext::filesystem::path& target);

		static bool isBuildBetter(const std::string& newstr, const std::string& oldstr, int minbuildtype);

		static bool doSelfUpdate(const std::string& buildname, const ext::filesystem::path& target, const ext::filesystem::path& selfpath);

		static std::vector<std::pair<std::string, std::string> > parseUpdateWebPage(const std::vector<uint8>& webpage);

		static bool doSigning(const ext::filesystem::path& keyfile, const std::string& tag, const std::string& build, const ext::filesystem::path& infile, const ext::filesystem::path& outfile);

	public: // actual object interface for offering builds for autoupdate

		void checkfile(boost::asio::io_service& service, const ext::filesystem::path& target, const std::string& bstring, bool signifneeded = false);

		boost::shared_ptr<std::vector<uint8> > getUpdateBuffer(const std::string& buildname) const;
		ext::filesystem::path getUpdateFilepath(const std::string& buildname) const;

		const std::vector<std::string>& getAvailableBuilds() const;

		boost::signals2::signal<void(std::string)> newbuild;

	private:
		struct buildinfo {
			ext::filesystem::path path;
			size_t len;
			std::vector<uint8> sig;
		};
		void addBuild(const std::string& build, boost::shared_ptr<buildinfo> info);
		void addBuild(const std::string& build, const ext::filesystem::path& path, size_t len, const std::vector<uint8>& sig);

		std::vector<std::string> buildstrings;
		std::map<std::string, boost::shared_ptr<buildinfo> > filedata;
};

#endif//AUTO_UPDATE_H
