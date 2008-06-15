#ifndef QT_LOGGER_H
#define QT_LOGGER_H

#include <iostream>
#include <sstream>
#include <string>

void qt_log_append(std::string);

#define LOG(msg) do { \
	std::ostringstream ss; \
	ss << nameonly(__FILE__) << ':' << __LINE__ << ':' << msg; \
	qt_log_append(ss.str()); \
} while(false)

#endif//QT_LOGGER_H