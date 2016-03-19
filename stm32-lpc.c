
#include <libopencm3/cm3/cortex.h>
#include <libopencm3/cm3/nvic.h>
#include <libopencm3/cm3/scb.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/crs.h>
#include <libopencm3/stm32/syscfg.h>
#include <libopencm3/stm32/gpio.h>
#include "extuart.h"
#include "usbcdc.h"

#define NOINLINE __attribute__((noinline))
#define NORETURN  __attribute__((noreturn))

#define PORT_LED GPIOF
#define PIN_LED GPIO0
#define led_set() gpio_set(PORT_LED, PIN_LED)
#define led_clear() gpio_clear(PORT_LED, PIN_LED)
#define led_toggle() gpio_toggle(PORT_LED, PIN_LED)

static void clocks_setup(void)
{
	/* Go Fast :) */
	rcc_clock_setup_in_hsi48_out_48mhz();

	/* Enable the SYSCFG so we can twiddle their bits. */
	rcc_periph_clock_enable(RCC_SYSCFG_COMP);
	/* Make sure the our flash is mapped to addr 0 for ISRs to work (re:DFU exit). */
	SYSCFG_CFGR1 = (SYSCFG_CFGR1 & ~SYSCFG_CFGR1_MEM_MODE) | SYSCFG_CFGR1_MEM_MODE_FLASH;

	/* Clock trimming from usb */
	rcc_periph_clock_enable(RCC_CRS);
	crs_autotrim_usb_enable();
	rcc_set_usbclk_source(RCC_HSI48);
}


static void gpio_setup(void)
{
	/* Enable all GPIOs in this device. */
	rcc_periph_clock_enable(RCC_GPIOA);
	rcc_periph_clock_enable(RCC_GPIOB);
	rcc_periph_clock_enable(RCC_GPIOF);

	gpio_mode_setup(GPIOB, GPIO_MODE_INPUT, GPIO_PUPD_PULLDOWN, GPIO8);
	gpio_mode_setup(PORT_LED, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, PIN_LED);
}



static void try_go_bootloader(void) {
	/* If BOOT1 is set, then just reset. */
	if (gpio_get(GPIOB, GPIO8)) {
		/* Signal bootloader entry with fast blink. */
		for(int n=0;n<20;n++) {
			led_toggle();
			for (int i = 0; i < 100000; i++) __asm__("nop");
		}
		led_set();
		scb_reset_system();
	}
	/* else nope, not allowed to bootload uselessly. */
}

void main(void) NORETURN NOINLINE;
void main(void)
{
	cm_disable_interrupts();
	clocks_setup();
	gpio_setup();
	extuart_init();
	DBG("Hi!\r\n");
	usbcdc_init();
	DBG("USB CDC Initialized\r\n");
	cm_enable_interrupts();
	for (;;) {
		DBG(".");
		/* Watch BOOT0 for changes. */
		uint16_t v = gpio_get(GPIOB, GPIO8);
		/* Blink the LED on the board. */
		led_set();
		while (v == gpio_get(GPIOB, GPIO8)) {
			led_toggle();
			DBG("L");
			for (int i = 0; i < 1000000; i++) __asm__("nop");
		}
		DBG("B");
		try_go_bootloader();
	}
}

static void signal_fault(void) NORETURN;
static void signal_fault(void) {
	DBG("\r\nFAULT\r\n");
	while (1) {
		uint16_t v = gpio_get(GPIOB, GPIO8);
		while (v == gpio_get(GPIOB, GPIO8));
		try_go_bootloader();
	}
}

void nmi_handler(void)
__attribute__ ((alias ("signal_fault")));

void hard_fault_handler(void)
__attribute__ ((alias ("signal_fault")));
