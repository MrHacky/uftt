#ifndef EXT_FILESYSTEM_H
#define EXT_FILESYSTEM_H

#include <boost/filesystem.hpp>

// different options for native filesystem format

// EXT_FILESYSTEM_UTF8_UTF8    // UTF8 is native format
// EXT_FILESYSTEM_UTF8_UTF16   // UTF16 is native format
// EXT_FILESYSTEM_UTF8_LOCALE  // native format determined by locale

#if defined(WIN32)
	#if defined(_WIN32_WINDOWS)
		//#define EXT_FILESYSTEM_UTF8_LOCALE // not working yet
		#define EXT_FILESYSTEM_UTF8_UTF8
	#else
		#define EXT_FILESYSTEM_UTF8_UTF16
	#endif
#elif defined(__APPLE__)
	#define EXT_FILESYSTEM_UTF8_UTF16
#else
	#define EXT_FILESYSTEM_UTF8_UTF8
	//#define EXT_FILESYSTEM_UTF8_LOCALE // can/should we detect linux UTF8 support?
#endif

namespace ext {
	namespace filesystem {
		namespace detail {
			#if 0
				struct UTF8TraitsUTF8;
				typedef boost::filesystem::basic_path<std::string, UTF8TraitsUTF8> UTF8PathUTF8;
				struct UTF8TraitsUTF8 {
					typedef UTF8PathUTF8 path_type;
					typedef boost::filesystem::path native_path_type;
					typedef std::string internal_string_type;
					typedef std::string external_string_type;
					static external_string_type to_external(const path_type &, const internal_string_type & src )
					{
						return src;
					}
					static internal_string_type to_internal(const external_string_type & src )
					{
						return src;
					}
				};
				typedef UTF8PathUTF8 UTF8PathNative;
			#elif defined(EXT_FILESYSTEM_UTF8_UTF8)
				typedef boost::filesystem::path UTF8PathNative;
			#elif defined(EXT_FILESYSTEM_UTF8_UTF16)
				struct UTF8TraitsUTF16;
				typedef boost::filesystem::basic_path< std::string, UTF8TraitsUTF16 > UTF8PathUTF16;
				struct UTF8TraitsUTF16 {
					typedef UTF8PathUTF16 path_type;
					typedef boost::filesystem::wpath native_path_type;
					typedef std::string internal_string_type;
					typedef std::wstring external_string_type;
					static external_string_type to_external(const path_type &, const internal_string_type & src );
					static internal_string_type to_internal(const external_string_type & src );
				};
				typedef UTF8PathUTF16 UTF8PathNative;
			#elif defined(EXT_FILESYSTEM_UTF8_LOCALE)
				struct UTF8TraitsLocale;
				typedef boost::filesystem::basic_path< std::string, UTF8TraitsLocale > UTF8PathLocale;
				struct UTF8TraitsLocale {
					typedef UTF8PathLocale path_type;
					typedef boost::filesystem::path native_path_type;
					typedef std::string internal_string_type;
					typedef std::string external_string_type;
					static external_string_type to_external(const path_type &, const internal_string_type & src );
					static internal_string_type to_internal(const external_string_type & src );
				};
				typedef UTF8PathLocale UTF8PathNative;
			#else
				#error "Define one of EXT_FILESYSTEM_UTF8_UTF8,EXT_FILESYSTEM_UTF8_UTF16,EXT_FILESYSTEM_UTF8_LOCALE"
			#endif
		}
		typedef detail::UTF8PathNative UTF8PathNative;
		typedef UTF8PathNative::traits_type UTF8TraitsNative;


		typedef UTF8PathNative path;
		typedef boost::filesystem::basic_directory_iterator<path> directory_iterator;
		typedef boost::filesystem::basic_recursive_directory_iterator<path> recursive_directory_iterator;
	}
}

#ifndef EXT_FILESYSTEM_UTF8_UTF8
// inform boost of our path class
namespace boost {
	namespace filesystem {
		template<> struct is_basic_path<ext::filesystem::path>
		{ BOOST_STATIC_CONSTANT( bool, value = true ); };
	}
}
#endif

namespace ext {
	namespace filesystem {
		bool exists(const path& ph);

		std::string external_utf8_file(const path& ph);
		std::string external_utf8_directory(const path& ph);

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
