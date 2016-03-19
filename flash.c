#include "main.h"
#include "flash.h"
#include "frser.h"
#include "uart.h"

static uint8_t flash_prot_in_use=0;

void flash_set_safe(void) {
}


void flash_select_protocol(uint8_t allowed_protocols) {
	allowed_protocols &= SUPPORTED_BUSTYPES;
#if 0
	flash_set_safe();
	if ((allowed_protocols&CHIP_BUSTYPE_LPC)&&(lpc_test())) {
		flash_prot_in_use = CHIP_BUSTYPE_LPC;
		return;
	}
	flash_set_safe();
	if ((allowed_protocols&CHIP_BUSTYPE_FWH)&&(fwh_test())) {
		flash_prot_in_use = CHIP_BUSTYPE_FWH;
		return;
	}
#endif
	flash_prot_in_use = 0;
	return;
}


uint8_t flash_read(uint32_t addr) {
	switch (flash_prot_in_use) {
		case 0:
		default:
		case CHIP_BUSTYPE_FWH:
		case CHIP_BUSTYPE_LPC:
			return 0xFF;
	}
}

void flash_readn(uint32_t addr, uint32_t len) {
	if (len==0) len = ((uint32_t)1<<24);
	switch (flash_prot_in_use) {
		case 0:
		default:
			while (len--) SEND(0xFF);
			return;
	}
}

void flash_write(uint32_t addr, uint8_t data) {
	switch (flash_prot_in_use) {
		case 0:
		default:
			return;
	}
}
