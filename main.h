#include <stdint.h>
#include "pgmspace-compat.h"
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <setjmp.h>
#include <libopencm3/stm32/gpio.h>

#define _delay_us udelay

#define NOINLINE __attribute__((noinline))
#define NORETURN  __attribute__((noreturn))
#define ALIGNED __attribute__((aligned(4)))
#define UNUSED __attribute__((unused))

#define PORT_LED GPIOF
#define PIN_LED GPIO0
#define led_set() gpio_set(PORT_LED, PIN_LED)
#define led_clear() gpio_clear(PORT_LED, PIN_LED)
#define led_toggle() gpio_toggle(PORT_LED, PIN_LED)
/* A thing to call when you're busy waiting. */
void yield(void);
void try_go_bootloader(void);
