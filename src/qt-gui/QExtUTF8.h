#ifndef QEXT_UTF8_H
#define QEXT_UTF8_H

#include <string>
#include <QString>
#include "../util/Filesystem.h"

namespace qext {
	namespace utf8 {
		inline QString toQString(const std::string& src)
		{
			return QString::fromUtf8(src.c_str());
		}

		inline std::string fromQString(QString src)
		{
			QByteArray utf8 = src.toUtf8();
			return std::string(utf8.begin(), utf8.end());
		}
	}

	namespace path {
		inline QString toQStringFile(const ext::filesystem::path& src)
		{
			return qext::utf8::toQString(ext::filesystem::external_utf8_file(src));
		}

		inline QString toQStringDirectory(const ext::filesystem::path& src)
		{
			return qext::utf8::toQString(ext::filesystem::external_utf8_directory(src));
		}

		inline ext::filesystem::path fromQString(QString src)
		{
			return ext::filesystem::path(qext::utf8::fromQString(src));
		}
	}
}

/** convert QString to std::string(utf8) */
inline std::string Q2S(const QString& s)
{
	return qext::utf8::fromQString(s);
}

/** convert std::string(utf8) to QString */
inline QString S2Q(const std::string& s)
{
	return qext::utf8::toQString(s);
}

#endif//QEXT_UTF8_H
