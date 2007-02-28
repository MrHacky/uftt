#ifdef NOTHING_TO_SEE_HERE_PEOPLE
	/**
	 * This shall contain all threads and thread related helpers/wrappers
	 *
	 */
	#include "stdafx.h"
	#include "yarn.h"

	template < typename T >
	THREAD spawnThread(int (WINAPI *start_routine)(T*), T *args) {

	#ifdef HAVE_PTHREAD_H
	  static pthread_attr_t attr; // Shared attributes for all threads (may be modified on a per-thread basis)
	  static bool attrNeedsInit = true;
	  THREAD t;
	  if(attrNeedsInit) {
		pthread_attr_init(&attr); //set implementation defined defaults
		pthread_attr_setdetachstate(&attr,PTHREAD_CREATE_DETACHED); //FIXME: Look up what 'Detached' means
		attrNeedsInit=false;
	  }
	  if(pthread_create(&t, &attr, (void*(*)(void*))start_routine, args) != THREAD_CREATE_OK) {
		fprintf(stderr, "Error while spawning new thread!\n");
		t=(THREAD)NULL;
	  }
	  return t;
	#else
	  THREAD thread_id;
	  HANDLE thread_handle;
	  thread_handle = CreateThread (NULL,
									0,
									(LPTHREAD_START_ROUTINE) start_routine,
									args,
									0,
									&thread_id
									);
	  return thread_id;
	#endif
	}

#endif
