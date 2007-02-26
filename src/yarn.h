#ifndef YARN_H
  #define YARN_H
  #include "stdafx.h"
  THREAD spawnThread(void *(*start_routine)(void*), void *args);
#endif
