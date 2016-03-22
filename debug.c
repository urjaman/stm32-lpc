#include <libopencm3/stm32/exti.h>
#include <libopencm3/cm3/nvic.h>
#include "main.h"
#include "extuart.h"
#include "usbcdc.h"
#include "debug.h"


#define DBG_EXTUART



static uint8_t usb_dbg_enabled = 1;
static uint8_t ser_dbg_enabled = 0;
static char usb_dbg_buf[USBCDC_PKT_SIZE_DAT] ALIGNED;

static uint8_t usb_dbg_cnt = 0;


void DBG(const char *str)
{
#ifdef DBG_EXTUART
	if (ser_dbg_enabled) extuart_sendstr(str);
#endif
#if 1
	if (usb_dbg_enabled) {
		int slen = strlen(str);
		int tlen = usb_dbg_cnt + slen;
		int dc = tlen > USBCDC_PKT_SIZE_DAT ? USBCDC_PKT_SIZE_DAT - usb_dbg_cnt : slen;
		memcpy(usb_dbg_buf + usb_dbg_cnt, str, dc);
		usb_dbg_cnt += dc;
		while (tlen >= USBCDC_PKT_SIZE_DAT) {
			if (usb_ready) {
				if (!usbcdc_write_ch3(usb_dbg_buf, usb_dbg_cnt)) return;
			} else {
				return;
			}
			usb_dbg_cnt = 0;
			slen -= dc;
			str += dc;
			tlen = slen;
			dc = slen > USBCDC_PKT_SIZE_DAT ? USBCDC_PKT_SIZE_DAT : slen;
			memcpy(usb_dbg_buf, str, dc);
			usb_dbg_cnt = dc;
		}
	}
#endif
}

void dbg_usb_flush(void)
{
	if ((usb_dbg_cnt)&&(usb_ready)) {
		if (!usbcdc_write_ch3(usb_dbg_buf, usb_dbg_cnt)) return;
		usb_dbg_cnt = 0;
	}
}



void dbg_present_val(const char* reg, uint32_t v)
{
	const char vals[16] = "0123456789ABCDEF";
	char buf[9];
	DBG(reg);
	buf[8] = 0;
	for (int i=7; i>=0; i--) {
		buf[i] = vals[ v & 0xF ];
		v = v >> 4;
	}
	DBG(buf);
}

void dbg_init(void)
{
	exti_set_trigger(EXTI8, EXTI_TRIGGER_RISING);
	exti_select_source(EXTI8, GPIOB);
	exti_reset_request(EXTI8);
	exti_enable_request(EXTI8);
	nvic_enable_irq(NVIC_EXTI4_15_IRQ);

}

static void fault_print(const char *f, uint32_t* fault_args)
{
#ifdef DBG_EXTUART
	usb_dbg_enabled = 0; // No trust in USB anymore
	ser_dbg_enabled = 1;
#endif
	DBG(f);
	dbg_present_val("PC: ", fault_args[6] );
	dbg_present_val(" LR: ", fault_args[5] );
	dbg_present_val(" PSR: ", fault_args[7] );
	dbg_present_val(" R12: ", fault_args[4] );
	dbg_present_val("\r\nR0: ", fault_args[0] );
	dbg_present_val(" R1: ", fault_args[1] );
	dbg_present_val(" R2: ", fault_args[2] );
	dbg_present_val(" R3: ", fault_args[3] );
#ifndef DBG_EXTUART
	dbg_usb_flush();
#endif


}


static void signal_fault(void) __attribute__((naked));
static void signal_fault(void)
{
	uint32_t *hardfault_args;
	asm volatile (
	        "movs r0,#4\n\t"
	        "movs r1, lr\n"
	        "tst r0, r1\n\t"
	        "beq 1f\n\t"
	        "mrs %[args], psp\n\t"
	        "b 2f\n\t"
	        "1:\n\t"
	        "mrs %[args], msp\n\t"
	        "2:\n\t"
	        : [args] "=r" (hardfault_args)
	);
	fault_print("\r\nFAULT\r\n", hardfault_args);


	while (1) {
		uint16_t v = gpio_get(GPIOB, GPIO8);
		while (v == gpio_get(GPIOB, GPIO8));
		try_go_bootloader();
	}
}


void boot_trap_isr(void) __attribute__((naked));
void boot_trap_isr(void) {
	uint32_t *args;
	asm volatile (
	        "movs r0,#4\n\t"
	        "movs r1, lr\n"
	        "tst r0, r1\n\t"
	        "beq 1f\n\t"
	        "mrs %[args], psp\n\t"
	        "b 2f\n\t"
	        "1:\n\t"
	        "mrs %[args], msp\n\t"
	        "2:\n\t"
	        : [args] "=r" (args)
	);
	fault_print("\r\nEXTI\r\n", args);

	try_go_bootloader();

	while (1) {
		uint16_t v = gpio_get(GPIOB, GPIO8);
		while (v == gpio_get(GPIOB, GPIO8));
		try_go_bootloader();
	}
}

void exti4_15_isr(void)
__attribute__ ((alias ("boot_trap_isr")));




void nmi_handler(void)
__attribute__ ((alias ("signal_fault")));

void hard_fault_handler(void)
__attribute__ ((alias ("signal_fault")));

