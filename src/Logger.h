#ifndef LOGGER_H
#define LOGGER_H

//#define LOG_TO_STDOUT
#define LOG_TO_QT

inline char* nameonly(char* str)
{
	for (char* loop = str; *loop; ++loop)
		if (*loop == '\\') str = loop;

	return str+1;
}

#ifdef LOG_TO_STDOUT
	#include <iostream>

	#define LOG(msg) std::cout << nameonly(__FILE__) << ':' << __LINE__ << ':' << msg << endl
#endif

#ifdef LOG_TO_QT
	#include "qt-gui/QtLogger.h"
#endif

#endif//LOGGER_H
