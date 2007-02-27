#ifndef YARN_H
  #define YARN_H
  #include "stdafx.h"

template < class T >
THREAD spawnThread(int *(WINAPI *start_routine)(T*), T *args);

#endif
