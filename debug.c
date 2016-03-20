#include "main.h"
#include "extuart.h"
#include "usbcdc.h"
#include "debug.h"


#define DBG_EXTUART



static char usb_dbg_buf[USBCDC_PKT_SIZE_DAT] ALIGNED;
static uint8_t usb_dbg_enabled = 1;
static uint8_t usb_dbg_cnt = 0;


void DBG(const char *str)
{
#ifdef DBG_EXTUART
	extuart_sendstr(str);
#endif
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
	usb_dbg_enabled = 0; // No trust in USB anymore

	DBG("\r\nFAULT\r\n");
	dbg_present_val("PC: ", hardfault_args[6] );
	dbg_present_val(" LR: ", hardfault_args[5] );
	dbg_present_val(" PSR: ", hardfault_args[7] );
	dbg_present_val(" R12: ", hardfault_args[4] );
	dbg_present_val("\r\nR0: ", hardfault_args[0] );
	dbg_present_val(" R1: ", hardfault_args[1] );
	dbg_present_val(" R2: ", hardfault_args[2] );
	dbg_present_val(" R3: ", hardfault_args[3] );

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
