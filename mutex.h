#ifndef _MUTEX_H_
#define _MUTEX_H_
#include <libopencm3/cm3/sync.h>


#if defined(__ARM_ARCH_7M__) || defined(__ARM_ARCH_7EM__)
/* This is all implemented in the sync stuff :) */

#else
/* CM0, we need to make our own mutex... */
 
typedef uint32_t mutex_t;

#define MUTEX_UNLOCKED 0
#define MUTEX_LOCKED	 1

uint32_t mutex_trylock(mutex_t *m);
void mutex_unlock(mutex_t *m);
/* We dont need the spin lock version, so we dont implement it either (trivial). */

#endif

#endif
