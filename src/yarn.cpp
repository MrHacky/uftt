/**
 * This shall contain all threads and thread related helpers/wrappers
 *
 */
#include "stdafx.h"
#include "yarn.h"

//
bool attrNeedsInit = true;
// Every thread spawned shall have it's own designated handle
THREAD tSpamSpam;
THREAD tReceiveSpam;

//Using void* is ok, since the actual call to pthread_create uses it
THREAD spawnThread(void *(WINAPI *start_routine)(void*), void *args) {
//THREAD spawnThread(unsigned long*  ( WINAPI *start_routine)(void* ), void *args) {
#ifdef HAVE_PTHREAD_H
  static pthread_attr_t attr; // Shared attributes for all threads (may be modified on a per-thread basis)
  THREAD t;
  if(attrNeedsInit) {
    pthread_attr_init(&attr); //set implementation defined defaults
    pthread_attr_setdetachstate(&attr,PTHREAD_CREATE_DETACHED); //FIXME: Look up what 'Detached' means
  }
  if(pthread_create(&t, &attr, start_routine, args) != THREAD_CREATE_OK) {
    fprintf(stderr, "Error while spawning new thread!\n");
    t=(THREAD)NULL;
  }
  return t;
#else
  THREAD t;
  LPDWORD tmp;
  t = CreateThread(
    0,
    0,
	(LPTHREAD_START_ROUTINE)
    start_routine,
    args,
    0,
    tmp
  );
  return t;
#endif

}
