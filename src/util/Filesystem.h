#ifndef EXT_FILESYSTEM_H
#define EXT_FILESYSTEM_H

#include <fstream>
#include <boost/filesystem.hpp>

// different options for native filesystem format

// EXT_FILESYSTEM_UTF8_UTF8    // UTF8 is native format
// EXT_FILESYSTEM_UTF8_UTF16   // UTF16 is native format
// EXT_FILESYSTEM_UTF8_LOCALE  // native format determined by locale

#if defined(WIN32)
	#if defined(_WIN32_WINDOWS)
		#define EXT_FILESYSTEM_UTF8_LOCALE
	#else
		#define EXT_FILESYSTEM_UTF8_UTF16
	#endif
#else
	// UNIX (or at least linux and mac osx)
	#define EXT_FILESYSTEM_UTF8_UTF8
	//#define EXT_FILESYSTEM_UTF8_LOCALE // can/should we detect UTF8 support?
#endif

namespace ext {
	namespace filesystem {
		#if 0
		#elif defined(EXT_FILESYSTEM_UTF8_UTF8)
			typedef boost::filesystem::path boostpath;
			typedef std::string externalstring;
		#elif defined(EXT_FILESYSTEM_UTF8_UTF16)
			typedef std::wstring externalstring;
			#if BOOST_FILESYSTEM_VERSION==3
				typedef boost::filesystem::path boostpath;
			#elif BOOST_FILESYSTEM_VERSION==2 || !defined(BOOST_FILESYSTEM_VERSION)
				typedef boost::filesystem::wpath boostpath;
			#else
				#error "Unsupported boost filesystem version"
			#endif
		#elif defined(EXT_FILESYSTEM_UTF8_LOCALE)
			typedef boost::filesystem::path boostpath;
			typedef std::string externalstring;
		#else
			#error "Define one of EXT_FILESYSTEM_UTF8_UTF8,EXT_FILESYSTEM_UTF8_UTF16,EXT_FILESYSTEM_UTF8_LOCALE"
		#endif

		#if BOOST_FILESYSTEM_VERSION==3
			typedef boost::filesystem::directory_iterator directory_iterator;
			typedef boost::filesystem::recursive_directory_iterator recursive_directory_iterator;
			typedef boost::filesystem::filesystem_error filesystem_error;
		#elif BOOST_FILESYSTEM_VERSION==2 || !defined(BOOST_FILESYSTEM_VERSION)
			typedef boost::filesystem::basic_directory_iterator<boostpath> directory_iterator;
			typedef boost::filesystem::basic_recursive_directory_iterator<boostpath> recursive_directory_iterator;
			typedef boost::filesystem::basic_filesystem_error<boostpath> filesystem_error;
		#else
			#error "Unsupported boost filesystem version"
		#endif

		class path {
			private:
				std::string value;

			public:
				path();
				path(const path& o);
				path(const char* o);
				path(const std::string& o);
				path(const boostpath& o);

				path& operator=(const path& o);
				path& operator=(const char* o);
				path& operator=(const std::string& o);
				path& operator=(const boostpath& o);

				operator boostpath() const;
				operator std::string() const;

				// external format: native separators/native encoding
				externalstring external_file_string() const;
				externalstring external_directory_string() const;

				// native format  : native seperators/utf8 encoding
				std::string native_file_string() const;
				std::string native_directory_string() const;

				// internal format: generic separators/utf8 encoding
				std::string string() const;

				bool operator==(const path& o) const;

				bool empty() const;

				path parent_path() const;
				std::string filename() const;

				path normalize() const;

				path operator/(const std::string& o) const;
				path operator/(const char* o) const;

				void operator/=(const std::string& o);
				void operator/=(const char* o);
		};

		template<typename OS>
		OS& operator<<(OS& os, const path& p) {
			return os << p.string();
		}
	}
}

namespace ext {
	namespace filesystem {
		bool exists(const path& ph);
		bool remove(const path& ph);
		path current_path();

		FILE* fopen(const ext::filesystem::path& path, const char* mode);

		class ifstream: public std::ifstream {
			public:
				ifstream(const ext::filesystem::path& path, std::ios_base::openmode mode = std::ios_base::in);
				void open(const ext::filesystem::path& path, std::ios_base::openmode mode = std::ios_base::in);
		};

		class ofstream: public std::ofstream {
			public:
				ofstream(const ext::filesystem::path& path, std::ios_base::openmode mode = std::ios_base::out);
				void open(const ext::filesystem::path& path, std::ios_base::openmode mode = std::ios_base::out);
		};
	}
}

#endif//EXT_FILESYSTEM_H
