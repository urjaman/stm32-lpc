#include <libopencm3/cm3/sync.h>
#include <libopencm3/cm3/cortex.h>
#include "mutex.h"


#if defined(__ARM_ARCH_7M__) || defined(__ARM_ARCH_7EM__)
/* This is all implemented in the sync stuff :) */

#else
/* CM0, we need to make our own mutex... */

uint32_t mutex_trylock(mutex_t *m_)
{
	uint32_t status = 1;
	volatile mutex_t * m = (volatile mutex_t*)m_;
	cm_disable_interrupts();
	if (*m == MUTEX_UNLOCKED) {
		*m = MUTEX_LOCKED;
		status = 0;
	}
	/* Execute the mysterious Data Memory Barrier instruction! */
	__dmb();
	cm_enable_interrupts();

	/* Did we get the lock? If not then try again
	 * by calling this function once more. */
	return status == 0;
}

void mutex_unlock(mutex_t *m_)
{
	volatile mutex_t * m = (volatile mutex_t*)m_;
	/* Ensure accesses to protected resource are finished */
	__dmb();
	/* Free the lock. */
	*m = MUTEX_UNLOCKED;
}

#endif
