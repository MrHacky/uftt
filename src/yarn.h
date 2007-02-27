#ifndef YARN_H
  #define YARN_H
  #include "stdafx.h"
  THREAD spawnThread(void *(WINAPI *start_routine)(void*), void *args);
#endif
