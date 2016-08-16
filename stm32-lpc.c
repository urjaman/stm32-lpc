
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
#include "debug.h"
#include "dmacopy.h"
#include "tasker.h"

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



void try_go_bootloader(void)
{
	/* If BOOT1 is set, then just reset. */
	if (gpio_get(GPIOB, GPIO8)) {
		/* Signal bootloader entry with fast blink. */
		for(int n=0; n<20; n++) {
			led_toggle();
			for (int i = 0; i < 100000; i++) __asm__("nop");
		}
		led_set();
		scb_reset_system();
	}
	/* else nope, not allowed to bootload uselessly. */
}

static uint16_t boot_gpio_val;


static int last_tcnt = 1;
void yield(void)
{
	int tcnt = task_count();
	if (tcnt != last_tcnt) {
		dbg_present_val("TASKS:", tcnt);
		last_tcnt = tcnt;
	}
	if (boot_gpio_val != gpio_get(GPIOB, GPIO8)) {
		try_go_bootloader();
		boot_gpio_val = gpio_get(GPIOB, GPIO8);
	}
	dbg_usb_flush();
	task_switch();
}

#define T2_STACK_SZ 128
static uint32_t t2_stack[T2_STACK_SZ/4] STACK_ALIGN;
void task2(void*n) {
	for(int i=0;i<8000;i++) {
		DBG("\r\nT2\r\n");
		task_switch();
	}
	DBG("\r\nT2 EXIT\r\n");
}


#define HANDLER_STACK_SZ 512
static uint32_t handler_stack[HANDLER_STACK_SZ/4] STACK_ALIGN;
void set_handler_stack(void *sp) __attribute__((naked));
void set_handler_stack(void *sp) {
	asm volatile(
	".syntax unified\n\t"
	"ldr r2, =#2\n\t"
	"mov r1, sp\n\t"
	"mov sp, %[nsp]\n\t"
	"mrs r0, CONTROL\n\t"
	"orrs r0, r2\n\t"
	"msr CONTROL, r0\n\t"
	"isb\n\t"
	"mov sp, r1\n\t"
	"bx lr\n\t"
	".pool\n\t"
	:: [nsp] "r" (sp)
	);
}


void main(void) NORETURN NOINLINE;
void main(void)
{
	cm_disable_interrupts();
	set_handler_stack(&(handler_stack[HANDLER_STACK_SZ/4]));
	clocks_setup();
	gpio_setup();
	extuart_init();
	dbg_init();
	DBG("Hi!\r\n");
	dmacpy_setup();
	usbcdc_init();
	DBG("USB CDC Initialized\r\n");
	cm_enable_interrupts();
	task_start(NULL,task2,&(t2_stack[T2_STACK_SZ/4]));
	boot_gpio_val = gpio_get(GPIOB, GPIO8);
	if (!usb_ready) {
		DBG("waiting for usb ready\r\n");
		while (!usb_ready) yield();
	}
	DBG("frser_main go\r\n");
	frser_main();
}


