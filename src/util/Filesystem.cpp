#include "Filesystem.h"

#include "../Platform.h"

namespace ext {
	namespace filesystem {
		namespace detail {
			#ifdef EXT_FILESYSTEM_UTF8_UTF16
				UTF8TraitsUTF16::external_string_type UTF8TraitsUTF16::to_external(const path_type &, const UTF8TraitsUTF16::internal_string_type & src )
				{
					return platform::convertUTF8ToUTF16(src);
				}

				UTF8TraitsUTF16::internal_string_type UTF8TraitsUTF16::to_internal(const UTF8TraitsUTF16::external_string_type & src )
				{
					return platform::convertUTF16ToUTF8(src);
				}
			#endif
			#ifdef EXT_FILESYSTEM_UTF8_LOCALE
				UTF8TraitsLocale::external_string_type UTF8TraitsLocale::to_external(const path_type &, const UTF8TraitsLocale::internal_string_type & src )
				{
					return platform::convertUTF8ToLocale(src);
				}

				UTF8TraitsLocale::internal_string_type UTF8TraitsLocale::to_internal(const UTF8TraitsLocale::external_string_type & src )
				{
					return platform::convertLocaleToUTF8(src);
				}
			#endif
		}

		namespace {
			template <typename S>
			inline FILE* utf8_fopen(const S& path, const char* mode);

			template <>
			inline FILE* utf8_fopen<std::string>(const std::string& path, const char* mode)
			{
				return ::fopen(path.c_str(), mode);
			}

			#ifdef WIN32
			template <>
			inline FILE* utf8_fopen<std::wstring>(const std::wstring& path, const char* mode)
			{
				std::string smode(mode);
				return ::_wfopen(path.c_str(), std::wstring(smode.begin(), smode.end()).c_str());
			}
			#endif // WIN32
		}

		bool exists(const path& ph) {
			try {
				return boost::filesystem::exists(ph);
			} catch (...) {
				return false;
			}
		}

		std::string external_utf8_file(const path& ph) {
			return path::traits_type::to_internal(ph.external_file_string());
		}

		std::string external_utf8_directory(const path& ph) {
			return path::traits_type::to_internal(ph.external_directory_string());
		}

		FILE* fopen(const ext::filesystem::path& path, const char* mode)
		{
			return utf8_fopen(path.external_file_string(), mode);
		}

		ifstream::ifstream(const ext::filesystem::path& path, std::ios_base::openmode mode)
			: std::ifstream(path.external_file_string().c_str(), mode)
		{
		}

		void ifstream::open(const ext::filesystem::path& path, std::ios_base::openmode mode)
		{
			std::ifstream::open(path.external_file_string().c_str(), mode);
		}

		ofstream::ofstream(const ext::filesystem::path& path, std::ios_base::openmode mode)
			: std::ofstream(path.external_file_string().c_str(), mode)
		{
		}

		void ofstream::open(const ext::filesystem::path& path, std::ios_base::openmode mode)
		{
			std::ofstream::open(path.external_file_string().c_str(), mode);
		}
	}
}
