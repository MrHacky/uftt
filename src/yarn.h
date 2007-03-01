#ifndef YARN_H
	#define YARN_H
	#include "stdafx.h"


	template < typename T >
	THREAD spawnThread(int (WINAPI *start_routine)(T*), T *args);


	//Since the 'export' keyword is not supported by GCC so we do it this way
	#define NOTHING_TO_SEE_HERE_PEOPLE
	#include "yarn.cpp"
	#undef NOTHING_TO_SEE_HERE_PEOPLE
#endif
