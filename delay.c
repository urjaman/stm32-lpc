#include "main.h"
#include "udelay.h"

void udelay(uint32_t us_loops) {
	const uint32_t fuzz = 3; /* Instructions counted from this function, plus call to this */
	us_loops *= 10; /* 48 Mhz / 5 cycles-per-loop => 9.6 => 10 */
	if (us_loops <= fuzz) return;
	us_loops -= fuzz;
	asm volatile (
		"1: \n\t"
		"SUB %[loops], #1\n\t"
		"CMP %[loops], #0\n\t"
		"BNE 1b \n\t" : : [loops] "r" (us_loops) : "memory"
	);
}
