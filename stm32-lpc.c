#include <libopencm3/cm3/nvic.h>
#include <libopencm3/cm3/scb.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/crs.h>
#include <libopencm3/stm32/syscfg.h>
#include <libopencm3/stm32/gpio.h>

static void clocks_setup(void)
{
	/* Go Fast :) */
	rcc_clock_setup_in_hsi48_out_48mhz();

	/* Enable the SYSCFG so we can twiddle their bits. */
	rcc_periph_clock_enable(RCC_SYSCFG_COMP);
	/* Make sure the our flash is mapped to addr 0 for ISRs to work (re:DFU exit). */
	SYSCFG_CFGR1 = (SYSCFG_CFGR1 & ~SYSCFG_CFGR1_MEM_MODE) | SYSCFG_CFGR1_MEM_MODE_FLASH;

	/* Clock trimming from usb */
	crs_autotrim_usb_enable();
	rcc_set_usbclk_source(RCC_HSI48);
}


#define PORT_LED GPIOF
#define PIN_LED GPIO0
static void gpio_setup(void)
{
	/* Enable all GPIOs in this device. */
	rcc_periph_clock_enable(RCC_GPIOA);
	rcc_periph_clock_enable(RCC_GPIOB);
	rcc_periph_clock_enable(RCC_GPIOF);

	gpio_mode_setup(GPIOB, GPIO_MODE_INPUT, GPIO_PUPD_PULLDOWN, GPIO8);
	gpio_mode_setup(PORT_LED, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, PIN_LED);
}

#define led_set() gpio_set(PORT_LED, PIN_LED)
#define led_clear() gpio_clear(PORT_LED, PIN_LED)
#define led_toggle() gpio_toggle(PORT_LED, PIN_LED)


static void try_go_bootloader(void) {
	/* If BOOT1 is set, then just reset. */
	if (gpio_get(GPIOB, GPIO8)) {
		/* Signal bootloader entry with fast blink. */
		for(int n=0;n<10;n++) {
			led_toggle();
			for (int i = 0; i < 100000; i++) __asm__("nop");
		}
		led_set();
		scb_reset_system();
	}
	/* else nope, not allowed to bootload uselessly. */
}

#define NORETURN  __attribute__((noreturn))
void main(void) NORETURN;
void main(void)
{
	gpio_setup();
	led_set();
	clocks_setup();
	led_clear();

	for (;;) {
		/* Watch BOOT0 for changes. */
		uint16_t v = gpio_get(GPIOB, GPIO8);
		/* Blink the LED (PC8) on the board. */
		while (v == gpio_get(GPIOB, GPIO8)) {
			led_toggle();
			for (int i = 0; i < 1000000; i++) __asm__("nop");
		}
		try_go_bootloader();
	}
}

