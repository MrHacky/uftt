#include "Filesystem.h"

#include "../Platform.h"

#include <boost/foreach.hpp>

namespace ext {
	namespace filesystem {
		namespace {
			inline FILE* utf8_fopen(const std::string& path, const char* mode)
			{
				return ::fopen(path.c_str(), mode);
			}

			#ifdef WIN32
			inline FILE* utf8_fopen(const std::wstring& path, const char* mode)
			{
				std::string smode(mode);
				return ::_wfopen(path.c_str(), std::wstring(smode.begin(), smode.end()).c_str());
			}
			#endif // WIN32

			// see remove() below
			struct voidtotrue {
				bool value;
				voidtotrue() : value(true) {};
				void operator,(bool v) { value = v; };
			};
		}

		bool exists(const path& ph) {
			try {
				return boost::filesystem::exists(ph);
			} catch (...) {
				return false;
			}
		}

		bool remove(const path& ph) {
			try {
				// magic trickery here: return true when boost::filesystem::remove has void return type,
				// but when it has bool return type, return whatever it returns
				voidtotrue vtt;
				// struct voidtotrue overloads operator,(bool) to store the bool in its .value member
				// but if remove returns void, the built-in operator,(void) gets chosen, which does nothing
				// and the initial true in .value gets preserved
				vtt, boost::filesystem::remove(ph);
				return vtt.value;
			} catch (...) {
				// swallow exceptions and return failure, if more detailed error info is needed
				// add and use remove(path, error_code) overload
				return false;
			}
		}

		path current_path()
		{
			#if BOOST_FILESYSTEM_VERSION==3
				return boost::filesystem::current_path();
			#elif BOOST_FILESYSTEM_VERSION==2 || !defined(BOOST_FILESYSTEM_VERSION)
				return boost::filesystem::current_path<boostpath>();
			#else
				#error "Unsupported boost filesystem version"
			#endif
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

namespace ext {
	namespace filesystem {
		namespace {
			std::string unknown_to_internal(std::string o)
			{
				#if defined(WIN32)
					BOOST_FOREACH(char& c, o)
						if (c == '\\') c = '/';
				#endif
				return o;
			}

			externalstring native_to_external(const std::string& o)
			{
				#if defined(EXT_FILESYSTEM_UTF8_UTF16)
					return platform::convertUTF8ToUTF16(o);
				#elif defined(EXT_FILESYSTEM_UTF8_LOCALE)
					return platform::convertUTF8ToLocale(o);
				#else
					return o;
				#endif
			}

			std::string boostpath_to_internal(const boostpath& o)
			{
				#if defined(EXT_FILESYSTEM_UTF8_UTF16)
					#if BOOST_FILESYSTEM_VERSION==3
						return platform::convertUTF16ToUTF8(o.generic_wstring());
					#elif BOOST_FILESYSTEM_VERSION==2 || !defined(BOOST_FILESYSTEM_VERSION)
						return platform::convertUTF16ToUTF8(o.string());
					#endif
				#elif defined(EXT_FILESYSTEM_UTF8_LOCALE)
					#if BOOST_FILESYSTEM_VERSION==3
						return platform::convertLocaleToUTF8(o.generic_string());
					#elif BOOST_FILESYSTEM_VERSION==2 || !defined(BOOST_FILESYSTEM_VERSION)
						return platform::convertLocaleToUTF8(o.string());
					#endif
				#else
					#if BOOST_FILESYSTEM_VERSION==3
						return o.generic_string();
					#elif BOOST_FILESYSTEM_VERSION==2 || !defined(BOOST_FILESYSTEM_VERSION)
						return o.string();
					#endif
				#endif
			}

			boostpath internal_to_boostpath(const std::string& o)
			{
				// skip internal->native because boost::fs:path will do that for us
				return boostpath(native_to_external(o));
			}

			std::string internal_to_native(std::string o)
			{
				#if defined(WIN32)
					BOOST_FOREACH(char& c, o)
						if (c == '/') c = '\\';
				#endif
				return o;
			}

			externalstring internal_to_external(const std::string& o)
			{
				return native_to_external(internal_to_native(o));
			}

		};

		path::path() {}

		path::path(const path& o)
		: value(o.value) {}

		path::path(const char* o)
		: value(unknown_to_internal(o)) {}

		path::path(const std::string& o)
		: value(unknown_to_internal(o)) {}

		path::path(const boostpath& o)
		: value(boostpath_to_internal(o)) {}

		path& path::operator=(const path& o)
		{
			value = o.value;
			return *this;
		}

		path& path::operator=(const char* o)
		{ return *this = path(o); }

		path& path::operator=(const std::string& o)
		{ return *this = path(o); }

		path& path::operator=(const boostpath& o)
		{ return *this = path(o); }

		path::operator boostpath() const
		{ return internal_to_boostpath(value); }

		path::operator std::string() const
		{ return this->string(); }

		boostpath path::bpath() const
		{ return internal_to_boostpath(value); }

		externalstring path::external_file_string() const
		{ return internal_to_external(value); }

		externalstring path::external_directory_string() const
		{ return internal_to_external(value); }

		std::string path::native_file_string() const
		{ return internal_to_native(value); }

		std::string path::native_directory_string() const
		{ return internal_to_native(value); }

		std::string path::string() const
		{ return value; }

		bool path::operator==(const path& o) const
		{ return value == o.value; }

		bool path::empty() const
		{ return value.empty(); }

		path path::operator/(const char* o) const
		{ path p(*this); p /= o; return p; }

		path path::operator/(const std::string& o) const
		{ path p(*this); p /= o; return p; }

		void path::operator/=(const char* o)
		{ *this /= std::string(o); }

		void path::operator/=(const std::string& o)
		{
			*this = path(this->bpath() / native_to_external(o));
			return;
			// TODO: test/profile more direct implementation below
			if (!value.empty() && *(value.end()-1) != '/')
				value.push_back('/');
			value += unknown_to_internal(o);
		}

		path path::parent_path() const
		{
			return path(this->bpath().branch_path());
			// TODO: test/profile more direct implementation below
			path ret;
			size_t lastslash = value.rfind('/');
			if (lastslash+1 == value.size())
				lastslash = value.rfind('/', lastslash-1);

			if (lastslash == std::string::npos)
				ret = path("");
			else
				ret = path(value.substr(0, lastslash));
			return ret;
		}

		std::string path::filename() const
		{
			return path(this->bpath().leaf()).string();
			// TODO: test/profile more direct implementation below
			std::string ret;
			size_t lastslash = value.rfind('/');
			if (lastslash+1 == value.size())
				lastslash = value.rfind('/', lastslash-1);
			if (lastslash == std::string::npos)
				ret =  value;
			else if (lastslash+1 == value.size())
				ret =  "/";
			else
				ret = value.substr(lastslash+1);
			return ret;
		}

		path path::normalize() const
		{
			return path(this->bpath().normalize());
			// TODO: figure out/test/profile more direct implementation
		}

	}
}
