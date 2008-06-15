#ifndef LOGGER_H
#define LOGGER_H

#include <iostream>

inline char* nameonly(char* str)
{
	for (char* loop = str; *loop; ++loop)
		if (*loop == '\\') str = loop;

	return str;
}

#define LOG(msg) std::cout << nameonly(__FILE__) << ':' << __LINE__ << ':' << msg << endl

#endif//LOGGER_H
