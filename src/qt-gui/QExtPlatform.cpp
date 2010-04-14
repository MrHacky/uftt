#include "QExtPlatform.h"

#include <stdexcept>

#include <QString>

namespace qext {
	namespace platform {
		std::string convertUTF8ToLocale(const std::string& src)
		{
			QString qs = QString::fromUtf8(src.c_str());
			QByteArray ba = qs.toLocal8Bit();
			return std::string(ba.begin(), ba.end());
		}

		std::string convertLocaleToUTF8(const std::string& src)
		{
			QString qs = QString::fromLocal8Bit(src.c_str());
			QByteArray ba = qs.toUtf8();
			return std::string(ba.begin(), ba.end());
		}
	}
}
