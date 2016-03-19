
#include <libopencm3/cm3/cortex.h>
#include <libopencm3/cm3/nvic.h>
#include <libopencm3/cm3/scb.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/crs.h>
#include <libopencm3/stm32/syscfg.h>
#include "main.h"
#include "extuart.h"
#include "usbcdc.h"
#include "frser.h"

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

static uint16_t boot_gpio_val;

void yield(void)
{
	if (boot_gpio_val != gpio_get(GPIOB, GPIO8)) {
		try_go_bootloader();
		boot_gpio_val = gpio_get(GPIOB, GPIO8);
	}
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
	boot_gpio_val = gpio_get(GPIOB, GPIO8);
	if (!usb_ready) {
		DBG("waiting for usb ready\r\n");
		while (!usb_ready) yield();
	}
	DBG("frser_main go\r\n");
	frser_main();
}

void signal_fault_c(uint32_t *hardfault_args) NORETURN __attribute__((externally_visible));
void signal_fault_c(uint32_t *hardfault_args) {
	DBG("\r\nFAULT\r\n");
	dbg_present_val("PC: ", hardfault_args[6] );
	dbg_present_val(" LR: ", hardfault_args[5] );
	dbg_present_val(" PSR: ", hardfault_args[7] );
	dbg_present_val(" R12: ", hardfault_args[4] );
	dbg_present_val("\r\nR0: ", hardfault_args[0] );
	dbg_present_val(" R1: ", hardfault_args[1] );
	dbg_present_val(" R2: ", hardfault_args[2] );
	dbg_present_val(" R3: ", hardfault_args[3] );

#if 0
	dbg_present_val("\r\nCFSR: ", SCB_CFSR );

	dbg_present_val(" HFSR: ", SCB_HFSR );
	dbg_present_val(" DFSR: ", SCB_DFSR );
	dbg_present_val(" AFSR: ", SCB_AFSR );

	dbg_present_val("\r\nMMFAR: ", SCB_MMFAR );
	dbg_present_val(" BFAR: ", SCB_BFAR );
#endif

	while (1) {
		uint16_t v = gpio_get(GPIOB, GPIO8);
		while (v == gpio_get(GPIOB, GPIO8));
		try_go_bootloader();
	}
}


static void signal_fault(void) __attribute__((naked));
static void signal_fault(void) {
  __asm volatile (
    " movs r0,#4       \n"
    " movs r1, lr      \n"
    " tst r0, r1       \n"
    " beq 1f         \n"
    " mrs r0, psp      \n"
    " b 2f          \n"
    "1:               \n"
    " mrs r0, msp      \n"
    "2:              \n"
    " ldr r1,[r0,#20]  \n"
    " b signal_fault_c \n"
    " bkpt #0          \n"
  );
}


void nmi_handler(void)
__attribute__ ((alias ("signal_fault")));

void hard_fault_handler(void)
__attribute__ ((alias ("signal_fault")));
